package com.tropicalcoder.autocord;

import android.content.Context;
import android.media.AudioAttributes;
import android.media.MediaPlayer;
import android.net.Uri;
import android.os.Handler;
import android.util.Log;
import android.widget.SeekBar;
import android.widget.TextView;
import android.widget.Toast;

import androidx.core.content.FileProvider;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileReader;
import java.io.IOException;

import static android.content.ContentValues.TAG;
import static android.media.MediaPlayer.SEEK_NEXT_SYNC;
import static android.media.MediaPlayer.SEEK_PREVIOUS_SYNC;

// See https://bugsdb.com/_en/debug/f56e1040e2e47b4c937c463801ada5f4 for seekbar code

public class AudioMediaPlayer
{
    private final int nSamplesPerMillisecond = 32000 / 1000;
    private final int nSamplesPerBuffer = 4096;
    private final int nMillisecondsPerBuffer = nSamplesPerBuffer / nSamplesPerMillisecond;
    private final int nPollingDelayMs = nMillisecondsPerBuffer / 2;

    // String array of event times eg "19:35:06"
    // Array has space for a time stamp for every 4K sample buffer
    // However, the vast majority will be null.
    // Thus we will be able to vector into the strings randomly
    // and find the time stamp for any point during play,
    // if a time stamp for that position was recorded in the log
    private String[] TimeStamps = null;
    private String[] PeakInfo = null;
    private String Path;

    private MediaPlayer mMediaPlayer = null;
    private final SeekBar seekBar;
    private final TextView tvTimestamp;
    private final TextView tvPeakFreq;
    private final TextView tvPeakDB;
    private final Context mContext;

    private int seekToPosition = 0;
    private int nCountBuffers = 0;
    private boolean isPrepared = false;
    private boolean isPaused = false;
    private boolean isSeeking = false;
    // boolean bPrepared = false;

    public AudioMediaPlayer(Context context, TextView timestamp, TextView peakFreq, TextView peakDB, SeekBar sb)
    {
        mContext = context;
        tvTimestamp = timestamp;
        tvPeakFreq = peakFreq;
        tvPeakDB = peakDB;
        seekBar = sb;
        seekBar.setOnSeekBarChangeListener(onSeekBarChangeListener);
    }

    // Assumes we have passed a non-null filename
    public void prepare(String pathName)
    {
        Path = pathName;
        if(mMediaPlayer != null)
        {
            stop();
        }
        isSeeking = false;
        seekToPosition = 0;
        seekBar.setProgress(0);
        isPrepared = createMediaPlayer(pathName);
        isPaused = true;
    }

    public boolean play()
    {
        if(!isPrepared)return false;

        if(isPaused)
        {
            isPaused = false;
            mMediaPlayer.start();
            mTimerHandler.postDelayed(mTimerRunnable, nPollingDelayMs);
            if(Debug.ON) Log.v("AudioTrack->play", "playing!");
        }
        return true;
    }

    private boolean createMediaPlayer(String pathName)
    {
        mMediaPlayer = new MediaPlayer();  // in the Idle state

        mMediaPlayer.setAudioAttributes(
                new AudioAttributes.Builder()
                        .setContentType(AudioAttributes.CONTENT_TYPE_SPEECH)
                        .setUsage(AudioAttributes.USAGE_MEDIA)
                        .build()
        );
        mMediaPlayer.setOnErrorListener(oel);
        mMediaPlayer.setOnPreparedListener(opl);
        mMediaPlayer.setLooping(true);
        mMediaPlayer.setOnSeekCompleteListener(oscl);
        // Was this,
        Uri uri = FileProvider.getUriForFile(mContext, mContext.getApplicationContext().getPackageName() + ".provider", new File(pathName));
        // This worked fine all along, but when making changes to the path for Android 10,
        // saw an exception in the log about DatabaseUtils: Writing exception to parcel java.lang.SecurityException: Permission Denial requires the provider be exported, requires the provider be exported, or grantUriPermission()
        // even though exporting and playing exported files worked fine.
        // Noted some code on StackOverflow did it like this...
        // Uri uri = FileProvider.getUriForFile(mContext, BuildConfig.APPLICATION_ID + ".provider", new File(pathName));
        // Tested - no more exception
        // Then put original code back to see if the exception returned - it didn't!
        try
        {
           mMediaPlayer.setDataSource(mContext, uri); // in Initialized state
        } catch (IOException e)
        {
           e.printStackTrace();
           mMediaPlayer.reset();
           Toast.makeText(mContext, "Sorry, can't open this file", Toast.LENGTH_LONG).show();
           return false;
        }

        mMediaPlayer.prepareAsync(); // prepare async to not block main thread
        return true;
    }

    // private class PlayerProcess implements Runnable
    private final Handler mTimerHandler = new Handler();
    private final Runnable mTimerRunnable = new Runnable()
    {
        @Override
        public void run()
        {
            if(!isPrepared || isPaused)return;

            int curPosMillis = mMediaPlayer.getCurrentPosition();

            if(isSeeking)
            {
                isSeeking = false;
                int newPos = seekToPosition * nMillisecondsPerBuffer;

                if(newPos != curPosMillis)
                {
                    int seekFlag;
                    if (newPos > curPosMillis)
                    {
                        seekFlag = SEEK_NEXT_SYNC;
                    } else
                    {
                        seekFlag = SEEK_PREVIOUS_SYNC;
                    }
                    mMediaPlayer.seekTo(newPos, seekFlag);
                    if (Debug.ON) Log.v("PlayerProcess", "skipped to: " + seekToPosition);
                    return;
                }
            }

            int progress = curPosMillis / nMillisecondsPerBuffer;

            if(progress != seekBar.getProgress())
            {
                seekBar.setProgress(progress);
                if (Debug.ON) Log.v("PlayerProcess", "seekBar.setProgress: " + progress);
                if ((TimeStamps != null) && (TimeStamps[progress] != null))
                {
                    tvTimestamp.setText(TimeStamps[progress]);
                    // if (Debug.ON) Log.v("PlayerProcess", TimeStamps[progress]);
                }else if (TimeStamps == null)
                {
                    tvTimestamp.setText("No log!");
                }

                if ((tvPeakFreq != null) && (PeakInfo != null) && (PeakInfo[progress] != null))
                {
                    // We should have "222.70 Hz,-74.5 dB"
                    int index = PeakInfo[progress].indexOf(",");
                    tvPeakFreq.setText(PeakInfo[progress].substring(0, index));
                    tvPeakDB.setText(PeakInfo[progress].substring(index + 1));
                }
            }

            if(!isPrepared || isPaused)return;

            mTimerHandler.postDelayed(mTimerRunnable, nPollingDelayMs);
        }
    };

    final MediaPlayer.OnSeekCompleteListener oscl = new MediaPlayer.OnSeekCompleteListener()
    {
        @Override
        public void onSeekComplete(MediaPlayer mp)
        {
            if (Debug.ON) Log.v(TAG, "Seek complete!");
            // mMediaPlayer.start();
            mTimerHandler.postDelayed(mTimerRunnable, nPollingDelayMs);
        }
    };

    final MediaPlayer.OnErrorListener oel = new MediaPlayer.OnErrorListener()
    {
        @Override
        public boolean onError(MediaPlayer mp, int what, int extra)
        {
            stop();
            // bPrepared = false;
            Toast.makeText(mContext, "Sorry, can't play this track. Error code: " + what + "." + extra, Toast.LENGTH_LONG).show();
            if (Debug.ON) Log.v(TAG, "Error code: " + what + "." + extra);
            return false;
        }
    };

    final MediaPlayer.OnPreparedListener opl = new MediaPlayer.OnPreparedListener()
    {
        @Override
        public void onPrepared(MediaPlayer player)
        {
            // if (Debug.ON) Log.v(TAG, "onPrepared"); // in the Prepared state
            int durationMillis = mMediaPlayer.getDuration();
            if (Debug.ON) Log.v(TAG, "durationMillis: " + durationMillis);
            int nFileSampleSize = durationMillis * nSamplesPerMillisecond;
            if (Debug.ON) Log.v(TAG, "nFileSampleSize: " + nFileSampleSize);
            nCountBuffers = nFileSampleSize / nSamplesPerBuffer;
            if((nFileSampleSize % nSamplesPerBuffer) > 0)
            {
                ++nCountBuffers;
            }
            if (Debug.ON) Log.v(TAG, "nCountBuffers: " + nCountBuffers);
            parseLog(Path);
            seekBar.setMax(nCountBuffers);
           // bPrepared = true;
        }
    };

    private void parseLog(String pathName)
    {
        int indexExtension = pathName.lastIndexOf(".");
        String fileName = pathName.substring(0, indexExtension);
        fileName = fileName.concat(".txt");
        if(Debug.ON)Log.v(TAG,"Log file name: " + fileName);

        FileReader fileReader;
        try
        {
            fileReader = new FileReader(fileName);
        } catch (FileNotFoundException e)
        {
            // Log file may have been deleted!
            e.printStackTrace();
            return;
        }
        BufferedReader bufferedReader = new BufferedReader(fileReader);

        int buffers = nCountBuffers;
        if(Debug.ON)Log.v(TAG, "buffers: " + buffers);
        TimeStamps = new String[buffers + 1];
        if(tvPeakFreq != null)
        {
            PeakInfo = new String[buffers + 1];
        }

        String s = "-";
        String currentTimeStamp;
        int countEvents = 0;

        while (!s.contains("Samples recorded"))
        {
            try
            {
                s = bufferedReader.readLine();
            } catch (IOException e)
            {
                e.printStackTrace();
            }
            if (s == null) break; // EOF
        }

        if(s != null)
        {
            String samplesRecorded = s.substring(19);
            if (Debug.ON) Log.v(TAG, samplesRecorded);
            int totalSamples = Integer.parseInt(samplesRecorded);
            if (Debug.ON) Log.v(TAG, "totalSamples: " + totalSamples);
            if (Debug.ON) Log.v(TAG, "total milliseconds: " + totalSamples / nSamplesPerMillisecond);
        }

        while(s != null)
        {
            while (!s.contains("Event time"))
            {
                try
                {
                    s = bufferedReader.readLine();
                } catch (IOException e)
                {
                    e.printStackTrace();
                }
                if (s == null) break; // EOF
            }

            if(s != null)
            {
                // if(Debug.ON)Log.v(TAG, s);

                // We have an event
                ++countEvents;
                currentTimeStamp = s.substring(11);
                // if(Debug.ON)Log.v(TAG, currentTimeStamp);
                try
                {
                    s = bufferedReader.readLine();
                } catch (IOException e)
                {
                    e.printStackTrace();
                }
                if (s == null) break; // EOF
                // s has the sample offset
                String sampleOffset = s.substring(15);
                // if(Debug.ON)Log.v(TAG, sampleOffset);
                int offset = Integer.parseInt(sampleOffset);
                // if(Debug.ON)Log.v(TAG, "offset: " + offset);
                int index = offset / nSamplesPerBuffer;
                // if(Debug.ON)Log.v(TAG, "index: " + index);
                if(index <= buffers)
                {
                    TimeStamps[index] = currentTimeStamp;
                }else break;

                if(tvPeakFreq != null)
                {
                    // Get peak info - ie: "Peak frequency: 222.70, dB: -74.5"
                    try
                    {
                        s = bufferedReader.readLine();
                    } catch (IOException e)
                    {
                        e.printStackTrace();
                    }
                    String infoRaw = s.substring(16); // copy part "222.70, dB: -74.5"
                    if(!infoRaw.equals(""))
                    {
                        int i = infoRaw.indexOf(",");
                        if (i > 0)
                        {
                            String info = infoRaw.substring(0, i); // Once got an exception here "String index out of range: -1" (was truncated file)
                            info = info.concat(" Hz,");
                            i = 2 + infoRaw.indexOf(":");
                            if (i > 0)
                            {
                                info = info.concat(infoRaw.substring(i)) + " dB";
                                // We should have "222.70 Hz,-74.5 dB"
                                PeakInfo[index] = info;
                            }
                        }
                    }
                }
            }
        }

        if(TimeStamps[0] != null)
        {
            tvTimestamp.setText(TimeStamps[0]);
            if (Debug.ON) Log.v(TAG, TimeStamps[0]);
        }
        if((tvPeakFreq != null) && (PeakInfo[0] != null))
        {
            int index = PeakInfo[0].indexOf(",");
            tvPeakFreq.setText(PeakInfo[0].substring(0, index));
            tvPeakDB.setText(PeakInfo[0].substring(index + 1));
        }

        if(Debug.ON)Log.v(TAG, "countEvents: " + countEvents);

    }



    public void pause()
    {
        if(isPrepared)
        {
            mMediaPlayer.pause();
            isPaused = true;
            mTimerHandler.removeCallbacks(mTimerRunnable);
            if(Debug.ON) Log.v("AudioTrack->pause", "pausing!");
        }
    }

    public boolean getPause()
    {
        return isPaused;
    }

    public void stop()
    {
        if (Debug.ON) Log.v("stop", "stopping!");
        isPrepared = false;
        isSeeking = false;
        mTimerHandler.removeCallbacks(mTimerRunnable);

        if (mMediaPlayer != null) {
            mMediaPlayer.stop();
            mMediaPlayer.reset();
            mMediaPlayer.release();
            mMediaPlayer = null;
        }
    }

    final SeekBar.OnSeekBarChangeListener onSeekBarChangeListener = new SeekBar.OnSeekBarChangeListener()
    {
        public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser)
        {
            // if (Debug.ON) Log.v("OnSeekBarChangeListener", "progress: " + progress + ", fromUser: " + fromUser);
            if(fromUser)
            {
                seekToPosition = progress;
                if((TimeStamps != null) && (TimeStamps[progress] != null))
                {
                    tvTimestamp.setText(TimeStamps[progress]);
                    // if (Debug.ON) Log.v("PlayerProcess", TimeStamps[progress]);
                }
                if((tvPeakFreq != null) && (PeakInfo != null) && (PeakInfo[progress] != null))
                {
                    // We should have "222.70 Hz,-74.5 dB"
                    int index = PeakInfo[progress].indexOf(",");
                    tvPeakFreq.setText(PeakInfo[progress].substring(0, index));
                    tvPeakDB.setText(PeakInfo[progress].substring(index + 1));
                }

                isSeeking = true;
            }
        }

        public void onStartTrackingTouch(SeekBar seekBar)
        {
        }

        public void onStopTrackingTouch(SeekBar seekBar)
        {
        }
    };
}
