package com.tropicalcoder.autocord;

import android.Manifest;
import android.content.ComponentName;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.ServiceConnection;
import android.content.SharedPreferences;
import android.content.pm.ActivityInfo;
import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.graphics.Color;
import android.graphics.Point;
import android.graphics.drawable.ColorDrawable;
import android.media.AudioAttributes;
import android.media.AudioFocusRequest;
import android.media.AudioManager;
import android.media.MediaPlayer;
import android.net.ConnectivityManager;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.os.PowerManager;
import android.text.Html;
import android.util.Log;
import android.view.Display;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.ImageButton;
import android.widget.ListView;
import android.widget.ProgressBar;
import android.widget.RadioButton;
import android.widget.SeekBar;
import android.widget.TextView;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.appcompat.app.ActionBar;
import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.ActivityCompat;
import androidx.core.content.FileProvider;
import androidx.core.content.res.ResourcesCompat;

import java.io.File;
import java.nio.ByteBuffer;
import java.util.Formatter;
import java.util.Objects;
import java.util.concurrent.Callable;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.ScheduledFuture;

import static android.content.pm.PackageManager.PERMISSION_DENIED;
import static android.content.pm.PackageManager.PERMISSION_GRANTED;
import static android.media.AudioManager.AUDIOFOCUS_GAIN;
import static java.lang.Math.floor;
import static java.lang.Thread.sleep;



public class MainActivity extends AppCompatActivity implements ServiceConnection, AudioService.CallBack, AudioManager.OnAudioFocusChangeListener
{
    private static final String AUTOCORD_SAVED_PREFS = "autocordsavedprefs";
    private static final String PEAK_AMPLITUDE_KEY = "peakamplitude";
    private static final String TOTAL_EVENTS_KEY = "totalevents";
    private static final String CONTIGUOUS_EVENTS_KEY = "contiguousevents";
    private static final String UI_MODE_KEY = "uimode";
    private static final String PREFILTER_KEY = "prefilter";
    private static final String INPUTBOOST_KEY = "inputboost";
    private static final String ENCODERTYPE_KEY = "encodertype";
    private static final String ENCODERBITRATE_KEY = "encoderbitrate";
    private static final String TRIGGERPRESET_KEY = "triggerpreset";

    private static final String TAG = MainActivity.class.toString();

    private static final int AUTOCORD_REQUEST = 0;

    enum PREFS_ACTION{GET, SET}
    public enum ToggleState { ON, OFF}

    // Modes
    private static final int INITIAL = 0;
    private static final int MONITOR = 1;
    private static final int RECORD = 2;
    private static final int PLAY = 3;
    private static final int EXPORT = 4;
    private static final int DESTROY = 5;

    public static final int AAC64K = 0;
    public static final int AAC128K = 1;
    public static final int FLAC = 2;

    public static final int TRIGGER_PRESET_VOICE = 0;
    public static final int TRIGGER_PRESET_BIRDS = 1;
    public static final int TRIGGER_PRESET_ANY = 2;

    public static final int STOP_TIMER = 2147483647;
    public static final int FRAMEDGRAPH_WIDTH = 482 * 2;
    public static final int FRAMEDGRAPH_HEIGHT = 322;

    public static ToggleState ToggleOnOffState = ToggleState.OFF;

    public static native void startEngine(float fPeakAmplitude);
    public static native float stopEngine();
    public static native float getGainApplied();
    public static native float getInputLevel();
    public static native int processData(boolean bFetchPeak);
    private native String getTimeStr();
    private native String getPeakInfo();
    private native String getMusicalNote();
    public static native void doFileIO(String pathname, int fileSizeBytes);
    private native void setMode(int nMode);
    private native void setPause(boolean bEnable);
    private native void setPrefilter(boolean bEnable);
    private native void setInputBoost(boolean bEnable);
    private native void passMediaPathname(String pathname);
    private native int getTotalEventsWrittenToLog();
    private native boolean checkExpired();
    private native void passEncodeBuffer(ByteBuffer buffer);

    static
    {
        System.loadLibrary("AudioEngine");
    }

    // CanvasView canvasView;
    private AudioManager.AudioRecordingCallback audioRecordingCallback;
    public static CanvasView customCanvas;
    public RadioButton radioMonitor;
    public RadioButton radioRecord;
    public RadioButton radioPlay;
    private ImageButton mbtnStart;
    private ImageButton mbtnStop;
    private ImageButton mbtnPause;
    private ImageButton mbtnUnPause;
    public TextView recordkb;
    public TextView recordtime;
    public TextView gainApplied;
    private TextView peakFreq;
    private TextView peakDB;
    private TextView musicalNote;
    private TextView currentMode;
    private View mOtionsView;
    public ProgressBar playProgress;
    public TrackDeleter trackDeleter = new TrackDeleter();
    private String savedCurrentModeText;
    private AudioMediaPlayer player = null;
    private AudioEncoder.ENCODER_TYPE encoderType = AudioEncoder.ENCODER_TYPE.AAC;
    public static PopSettings.PRESETS triggerPreset = PopSettings.PRESETS.VOICE;
    private static int encoderBitRate = 64000;
    private float fPeakAmplitude = 1.f;
    private static int curMode = INITIAL;
    private static int savedMode = INITIAL;

    private int currentAudioFileLength = 0;
    private static int dataWritten = 0;
    private int countTotalEvents = 0;
    private int countContiguousEventGroups = 0;
    private boolean bBeganContiguousEventsGroup = false;
    private boolean bPauseToggle = false;
    private boolean bTimerStopping = false;
    private boolean bTimerSleeping = true;
    private boolean bExitApp = false;
    private boolean bUIModeSimplified = true;
    private boolean bPrefilterOn = true;
    private boolean bInputBoostOn = false;
    public static boolean bScreenIsOn = true;
    static boolean bTimerWakeUp = false;
    private PowerManager.WakeLock wakeLock;

    BroadcastRcvr broadcastrcvr;
    AudioManager audioManager;
    AudioService audioService;
    AudioStorage audioStorage;
    LogFileAccess logFileAccess;
    BluetoothManager bluetoothManager;

    @Override
    public void onServiceConnected(ComponentName name, IBinder service)
    {
        if(Debug.ON)Log.v("onServiceConnected", "connected");
        audioService = ((AudioService.MyBinder)service).getService();
        audioService.setCallBack(this);
    }

    public static boolean isCapturingAudio()
    {
        return (ToggleOnOffState == ToggleState.ON) && ((curMode == RECORD) || (curMode == MONITOR));
    }

    @Override
    public void onServiceDisconnected(ComponentName name)
    {
        if(Debug.ON)Log.v("onServiceDisconnected", "disconnected");
        audioService = null;
    }

    @Override
    public void onOperationProgress(int progress)
    {
        if(Debug.ON)Log.v("onOperationProgress", "progress: " + progress);
    }

    @Override
    public void onOperationCompleted(int user)
    {
        if(Debug.ON)Log.v("onOperationCompleted", "completed");
    }

    public void onClickBtnPopup(View view)
    {
        startActivity(new Intent(MainActivity.this, PopSettings.class));
        // customCanvas.ClearBitmap(); // for testing something
    }

    public void onClickBtnInfo(View view)
    {
        AudioInfoSharedPrefsOp(PREFS_ACTION.GET);  // To get fPeakAmplitude, contiguousEventGroups, and TotalEvents
        ShowLogfileHeader();
    }

    public void onTogglePause(View view)
    {
        if(view != null)
        {
            // Called from user click
            if (ToggleOnOffState == ToggleState.OFF) return;
        } // else we were called from code, wanting to unpause
        bPauseToggle = !bPauseToggle;
        if(bPauseToggle)
        {
            if(ToggleOnOffState == ToggleState.ON)
            {
                setTimer(STOP_TIMER);
            }
            mbtnUnPause.setVisibility(View.VISIBLE);
            mbtnPause.setVisibility(View.INVISIBLE);
            if(Debug.ON)Log.d(TAG, "Paused!");
        }else
        {
            if(ToggleOnOffState == ToggleState.ON)
            {
                setTimer(1);
            }
            mbtnPause.setVisibility(View.VISIBLE);
            mbtnUnPause.setVisibility(View.INVISIBLE);
            if(Debug.ON)Log.d(TAG, "Unpaused!");
        }
        setPause(bPauseToggle);
    }


    private void AudioInfoSharedPrefsOp(PREFS_ACTION action)
    {
        if(action == PREFS_ACTION.SET)
        {
            if(fPeakAmplitude == 0)fPeakAmplitude = 1.f; // minimal sanity check to avoid potential div zero error later
            if(Debug.ON)Log.v("SaveSharedPrefs", "fPeakAmplitude: " + fPeakAmplitude);
            SharedPreferences.Editor editor = getSharedPreferences(AUTOCORD_SAVED_PREFS, MODE_PRIVATE).edit();
            editor.putFloat(PEAK_AMPLITUDE_KEY, fPeakAmplitude);
            editor.putLong(TOTAL_EVENTS_KEY,  countTotalEvents);
            editor.putLong(CONTIGUOUS_EVENTS_KEY, countContiguousEventGroups);
            editor.commit();
        }else // (action == PREFS_ACTION.GET)
        {
            // Retrieve data from preference:
            SharedPreferences prefs = getSharedPreferences(AUTOCORD_SAVED_PREFS, MODE_PRIVATE);
            if (prefs.contains(PEAK_AMPLITUDE_KEY))
            {
                fPeakAmplitude = prefs.getFloat(PEAK_AMPLITUDE_KEY, 1.f);
                if(Debug.ON)Log.v("GetSharedPrefs", "fPeakAmplitude: " + fPeakAmplitude);
            }
            if (prefs.contains(TOTAL_EVENTS_KEY))
            {
                countTotalEvents = (int)prefs.getLong(TOTAL_EVENTS_KEY, 0);
            }

            if (prefs.contains(CONTIGUOUS_EVENTS_KEY))
            {
                countContiguousEventGroups = (int)prefs.getLong(CONTIGUOUS_EVENTS_KEY, 0);
            }
        }
    }

    private void UIModeSharedPrefsOp(PREFS_ACTION action)
    {
        if(action == PREFS_ACTION.SET)
        {
            SharedPreferences.Editor editor = getSharedPreferences(AUTOCORD_SAVED_PREFS, MODE_PRIVATE).edit();
            editor.putBoolean(UI_MODE_KEY, bUIModeSimplified);
            editor.commit();
        }else // (action == PREFS_ACTION.GET)
        {
            // Retrieve data from preference:
            SharedPreferences prefs = getSharedPreferences(AUTOCORD_SAVED_PREFS, MODE_PRIVATE);
            if (prefs != null)
            {
                if(prefs.contains(UI_MODE_KEY))
                {
                    bUIModeSimplified = prefs.getBoolean(UI_MODE_KEY, true);
                }
            }
        }
    }

    private void PrefilterPrefsOp(PREFS_ACTION action)
    {
        if(action == PREFS_ACTION.SET)
        {
            SharedPreferences.Editor editor = getSharedPreferences(AUTOCORD_SAVED_PREFS, MODE_PRIVATE).edit();
            editor.putBoolean(PREFILTER_KEY, bPrefilterOn);
            editor.commit();
        }else // (action == PREFS_ACTION.GET)
        {
            // Retrieve data from preference:
            SharedPreferences prefs = getSharedPreferences(AUTOCORD_SAVED_PREFS, MODE_PRIVATE);
            if (prefs != null)
            {
                if(prefs.contains(PREFILTER_KEY))
                {
                    bPrefilterOn = prefs.getBoolean(PREFILTER_KEY, true);
                }
            }
        }
    }

    private void InputBoostPrefsOp(PREFS_ACTION action)
    {
        if(action == PREFS_ACTION.SET)
        {
            if(Debug.ON)Log.v(TAG, "InputBoostPrefsOp SET");
            SharedPreferences.Editor editor = getSharedPreferences(AUTOCORD_SAVED_PREFS, MODE_PRIVATE).edit();
            editor.putBoolean(INPUTBOOST_KEY, bInputBoostOn);
            editor.commit();
        }else // (action == PREFS_ACTION.GET)
        {
            if(Debug.ON)Log.v(TAG, "InputBoostPrefsOp GET");
            // Retrieve data from preference:
            SharedPreferences prefs = getSharedPreferences(AUTOCORD_SAVED_PREFS, MODE_PRIVATE);
            if (prefs != null)
            {
                if(prefs.contains(INPUTBOOST_KEY))
                {
                    bInputBoostOn = prefs.getBoolean(INPUTBOOST_KEY, false);
                }
            }
        }
    }

    private void TriggerPresetPrefsOp(PREFS_ACTION action)
    {
        int keyvalue = 0;

        if(action == PREFS_ACTION.SET)
        {
            if(triggerPreset == PopSettings.PRESETS.VOICE)
            {
               keyvalue = TRIGGER_PRESET_VOICE;
            }else if(triggerPreset == PopSettings.PRESETS.BIRDS)
            {
               keyvalue = TRIGGER_PRESET_BIRDS;
            }else if(triggerPreset == PopSettings.PRESETS.ANY)
            {
               keyvalue = TRIGGER_PRESET_ANY;
            }

            if(Debug.ON)Log.v(TAG, "TriggerPresetPrefsOp SET");
            SharedPreferences.Editor editor = getSharedPreferences(AUTOCORD_SAVED_PREFS, MODE_PRIVATE).edit();
            editor.putInt(TRIGGERPRESET_KEY, keyvalue);
            editor.commit();
        }else // (action == PREFS_ACTION.GET)
        {
            if(Debug.ON)Log.v(TAG, "TriggerPresetPrefsOp GET");
            // Retrieve data from preference:
            SharedPreferences prefs = getSharedPreferences(AUTOCORD_SAVED_PREFS, MODE_PRIVATE);
            if (prefs != null)
            {
                if(prefs.contains(TRIGGERPRESET_KEY))
                {
                    keyvalue = prefs.getInt(TRIGGERPRESET_KEY, TRIGGER_PRESET_VOICE);
                    if(keyvalue == TRIGGER_PRESET_VOICE)
                    {
                        triggerPreset = PopSettings.PRESETS.VOICE;
                    }else if(keyvalue == TRIGGER_PRESET_BIRDS)
                    {
                        triggerPreset = PopSettings.PRESETS.BIRDS;
                    }else if(keyvalue == TRIGGER_PRESET_ANY)
                    {
                        triggerPreset = PopSettings.PRESETS.ANY;
                    }
                }
            }
        }
    }

    private void EncoderTypePrefsOp(PREFS_ACTION action)
    {
        int keyvalue = 0;

        if(action == PREFS_ACTION.SET)
        {
            if(encoderType == AudioEncoder.ENCODER_TYPE.AAC)
            {
                if(encoderBitRate == 64000)
                {
                    keyvalue = AAC64K;
                }else if(encoderBitRate == 128000)
                {
                    keyvalue = AAC128K;
                }
            }else if(encoderType == AudioEncoder.ENCODER_TYPE.FLAC)
            {
                keyvalue = FLAC;
            }

            if(Debug.ON)Log.v(TAG, "EncoderTypePrefsOp SET");
            SharedPreferences.Editor editor = getSharedPreferences(AUTOCORD_SAVED_PREFS, MODE_PRIVATE).edit();
            editor.putInt(ENCODERTYPE_KEY, keyvalue);
            editor.commit();
        }else // (action == PREFS_ACTION.GET)
        {
            if(Debug.ON)Log.v(TAG, "EncoderTypePrefsOp GET");
            // Retrieve data from preference:
            SharedPreferences prefs = getSharedPreferences(AUTOCORD_SAVED_PREFS, MODE_PRIVATE);
            if (prefs != null)
            {
                if(prefs.contains(ENCODERTYPE_KEY))
                {
                    keyvalue = prefs.getInt(ENCODERTYPE_KEY, AAC64K);
                    if(keyvalue == AAC64K)
                    {
                        encoderType = AudioEncoder.ENCODER_TYPE.AAC;
                        encoderBitRate = 64000;
                    }else if(keyvalue == AAC128K)
                    {
                        encoderType = AudioEncoder.ENCODER_TYPE.AAC;
                        encoderBitRate = 128000;
                    }else if(keyvalue == FLAC)
                    {
                        encoderType = AudioEncoder.ENCODER_TYPE.FLAC;
                    }
                }
            }
        }
    }

    // Dependent on processData() having been called immediately before this
    // Show peak frequency, dB level, and musical note value
    private void ShowPeakInfo()
    {
        // New with version 1.15 - show the averaged peak level of input stream
        // This is different than level of peak frequency, being derived from
        // peak amplitude per buffer of incomming audio, smoothed.
        if((curMode == MONITOR) || (curMode == RECORD))
        {
            StringBuilder sbuf = new StringBuilder();
            Formatter fmt = new Formatter(sbuf);
            float fInputLevel = getInputLevel();
            if (fInputLevel == 0)
            {
                fmt.format("0 dB");
            } else
            {
                fmt.format("%1.1f dB", fInputLevel);
            }
            gainApplied.setText(sbuf.toString());
            sbuf.delete(0, sbuf.length());
            // Drive VU meter...
            fInputLevel = (96.3f + fInputLevel) - 26.3f;
            if(fInputLevel < 0)fInputLevel = 0;
            if(fInputLevel > 70)fInputLevel = 70;
            playProgress.setProgress((int)fInputLevel);
        }

        String sPeakInfo = getPeakInfo();
        if ((sPeakInfo != null) && !sPeakInfo.equals(""))
        {
            int indexOfFreq = sPeakInfo.indexOf(":");
            int indexOfDB = sPeakInfo.indexOf("dB");
            if((indexOfFreq > 0) && (indexOfDB > 0))
            {
                String freq = sPeakInfo.substring(indexOfFreq + 1, indexOfDB - 2);
                if(!freq.equals(""))
                {
                    freq = freq.concat(" Hz");
                    peakFreq.setText(freq);

                    // Parse out the dB value
                    String sDB;
                    // Is there a CR at the end? We get that on Play 'cause string comes from file
                    if (sPeakInfo.charAt(sPeakInfo.length() - 1) == '\n')
                    {
                        sDB = sPeakInfo.substring(indexOfDB + 4, sPeakInfo.length() - 1);
                    } else
                    {
                        sDB = sPeakInfo.substring(indexOfDB + 4);
                    }
                    sDB = sDB.concat(" dB");
                    peakDB.setText(sDB);
                }
            }
        }

    }

    private void SetUIMode(boolean bSimplified, boolean bOnUIModeRadioButtonClicked)
    {
        bUIModeSimplified = bSimplified;
        if(bUIModeSimplified)
        {
            TextView tv = findViewById(R.id.labelPeakFreq);
            tv.setVisibility(View.INVISIBLE);
            tv = findViewById(R.id.labelPeakDB);
            tv.setVisibility(View.INVISIBLE);
            tv = findViewById(R.id.labelMusicalNote);
            tv.setVisibility(View.INVISIBLE);
            tv = findViewById(R.id.peakFreq);
            tv.setVisibility(View.INVISIBLE);
            tv = findViewById(R.id.peakDB);
            tv.setVisibility(View.INVISIBLE);
            tv = findViewById(R.id.musicalNote);
            tv.setVisibility(View.INVISIBLE);
        }else
        {
            TextView tv = findViewById(R.id.labelPeakFreq);
            tv.setVisibility(View.VISIBLE);
            tv = findViewById(R.id.labelPeakDB);
            tv.setVisibility(View.VISIBLE);
            tv = findViewById(R.id.labelMusicalNote);
            tv.setVisibility(View.VISIBLE);
            tv = findViewById(R.id.peakFreq);
            tv.setVisibility(View.VISIBLE);
            tv = findViewById(R.id.peakDB);
            tv.setVisibility(View.VISIBLE);
            tv = findViewById(R.id.musicalNote);
            tv.setVisibility(View.VISIBLE);
        }
    }

    private void UpdateUI(int countSamplesWrittenToDisk)
    {
        if (bScreenIsOn)
        {
            String sMusicalNote = getMusicalNote();
            int colour, alphaColour = musicalNote.getCurrentTextColor();
            colour = alphaColour & 0x00FFFFFF;
            if ((sMusicalNote != null) && !sMusicalNote.equals(""))
            {
                // Log.v(TAG, sMusicalNote);
                musicalNote.setText(sMusicalNote);
                // Restore the full alpha channel
                colour |= 0xFF000000;
                musicalNote.setTextColor(colour);
            } else
            {
                // Reduce the alpha
                // int alpha = alphaColour & 0xFF000000;
                int alpha = (alphaColour >> 24) & 0xFF;
                if (alpha >= 8)
                {
                    alpha -= 8;
                    colour |= (alpha << 24);
                    musicalNote.setTextColor(colour);
                }
            }
        }

        if(countSamplesWrittenToDisk != -1) // Returns -1 when stopped
        {
            ShowPeakInfo(); // new with vers. 1.17!
            if (curMode == PLAY)
            {
                ShowRecordFileStats(countSamplesWrittenToDisk);
            } else if (curMode == RECORD)
            {
                // in RECORD mode, processData() only returns countSamplesWrittenToDisk when triggered to record
                if (countSamplesWrittenToDisk > 0)
                {
                    currentMode.setTextColor(Color.RED);
                    ShowRecordFileStats(countSamplesWrittenToDisk);
                    if(!bBeganContiguousEventsGroup)
                    {
                        ++countContiguousEventGroups;
                        bBeganContiguousEventsGroup = true;
                    }
                } else
                {
                    currentMode.setTextColor(ResourcesCompat.getColor(getResources(), R.color.colorTealAccent2, null));
                    if (bBeganContiguousEventsGroup)
                    {
                        // End contiguous samples group
                        // We may never get here if we stopped while recording was triggered
                        if(Debug.ON)Log.d(TAG, "Detected end of group!");
                        bBeganContiguousEventsGroup = false;
                    }
                }
            }
            if (bScreenIsOn)
            {
                customCanvas.Redraw(); // Must be preceded by processData()
            }
        }else
        {
            customCanvas.Redraw();
        } // End if(countSamplesWrittenToDisk != -1)
    }

    public void onExportFile(View view)
    {
        if(ToggleOnOffState == ToggleState.ON)
        {
            Toast.makeText(MainActivity.this, "Stop current activity first", Toast.LENGTH_LONG).show();
            return;
        }


        String pathRecordedData = getAudioFilePath(getApplicationContext(), false);
        if(curMode != PLAY)
        {
            if (currentAudioFileLength < 32768)
            {
                Toast.makeText(MainActivity.this, "There is no audio data to export!", Toast.LENGTH_LONG).show();
                return;
            }
        }
/*
        if(view != null)
        {

            if (!isWriteExternalStoragePermissionGranted())
            {
                if(Debug.ON)Log.v("Manifest.permission", "Don't have permission for WRITE_EXTERNAL_STORAGE");
                if (shouldShowRequestPermissionRationale(Manifest.permission.WRITE_EXTERNAL_STORAGE))
                {
                    Toast.makeText(MainActivity.this, "App needs access to store audio files!", Toast.LENGTH_LONG).show();
                }
                requestWriteExternalStoragePermission();
                return;
                // On permission granted as detected in onRequestPermissionsResult(),
                // we call this function again with view = null
            }

            //  Once we asked for WRITE_EXTERNAL_STORAGE permission,
            //  we should automatically have permission for READ_EXTERNAL_STORAGE
        }
*/

        if(curMode == PLAY)
        {
            // We are going to display the exported audio files
            // User will be able to select from a list to play them
            if(audioStorage.haveExportedAudio())
            {
                if (Debug.ON) Log.v(TAG, "Calling PlayExportedAudio");
                EnumReturn mFilesList = audioStorage.EnumerateExportedAudio();
                if(mFilesList == null)
                {
                    // Don't expect this because we just checked that we haveExportedAudio()
                    return;
                }
                PlayExportedAudio(mFilesList);
            }else
            {
                Toast.makeText(MainActivity.this, "You don't have any exported audio files yet!", Toast.LENGTH_LONG).show();
            }
            return;
        }

        // We are going to export an audio file
        AudioInfoSharedPrefsOp(PREFS_ACTION.GET);  // To get fPeakAmplitude, contiguousEventGroups, and TotalEvents
        showExportDialog(pathRecordedData);
    }



    private final Handler mTimerHandler = new Handler();
    private final Runnable mTimerRunnable = new Runnable()
    {
        @Override
        public void run()
        {
           // Log.v("Runnable", "Sleep now");
           while(bTimerSleeping)
           {
               try
               {
                   sleep(1);
               } catch (InterruptedException e)
               {
                   e.printStackTrace();
               }

               if(bExitApp)
               {
                   setTimer(STOP_TIMER);
                   moveTaskToBack(true);
                   android.os.Process.killProcess(android.os.Process.myPid());
                   System.exit(1);
               }

               if (bTimerWakeUp)
               {
                   if(curMode != EXPORT)
                   {
                       int countSamplesWrittenToDisk = processData(false);
                       UpdateUI(countSamplesWrittenToDisk);
                   }else // curMode == EXPORT
                   {
                       UpdateEncoderProgress(dataWritten);
                   }
                   bTimerWakeUp = false;
                   bTimerSleeping = false;
               } // End if (bTimerWakeUp)
           } // End while(bTimerSleeping)

           if(!bTimerStopping)
           {
               setTimer(1);
           }
        }
    };

    private void ShowLogfileHeader()
    {
        String[] sLogHeader = logFileAccess.ReadLogHeader(this, currentAudioFileLength, fPeakAmplitude, countContiguousEventGroups, countTotalEvents);
        if(sLogHeader == null)return;
        AlertDialog.Builder builder = new AlertDialog.Builder(MainActivity.this);
        View mView = LayoutInflater.from(MainActivity.this).inflate(R.layout.dialog_list, null);
        final ListView lvListTimes = mView.findViewById(R.id.list_logheader); // list_logheader is the ID of the list box in dialog_list

        TextView header = (TextView)getLayoutInflater().inflate(R.layout.list_header, lvListTimes, false);
        lvListTimes.addHeaderView(header);
        ArrayAdapter<String> adapter = new ArrayAdapter<>(this, R.layout.list_row_dark, R.id.listTextViewDark, sLogHeader);
        lvListTimes.setAdapter(adapter);

        builder.setView(mView)
                // Add action buttons
                .setPositiveButton(getResources().getString(R.string.close), (dialog, id) -> dialog.cancel());
        new DialogInterface.OnClickListener()
        {
            public void onClick(DialogInterface dialog, int id)
            {
                dialog.cancel();
            }
        };

        AlertDialog alertDialog = builder.create();
        alertDialog.show();
    }

    public void ShowRecordFileStats(int countSamplesWrittenToDisk)
    {
        StringBuilder sbuf = new StringBuilder();
        Formatter fmt = new Formatter(sbuf);

        if(curMode == RECORD)
        {
            double dSeconds = (double) countSamplesWrittenToDisk / 32000;
            int seconds = (int) floor(dSeconds + 0.5);
            int minutes = seconds / 60;
            int hours = minutes / 60;

            seconds %= 60;
            minutes %= 60;
            hours %= 100; // // What if >= 100 hours? We roll over to start at 0 hours again

            fmt.format("%02d:%02d:%02d", hours, minutes, seconds);
            recordtime.setText(sbuf.toString());
            sbuf.delete(0, sbuf.length());
        }else if(curMode == PLAY)
        {
            if(countSamplesWrittenToDisk <= 0)
            {
                countSamplesWrittenToDisk = -countSamplesWrittenToDisk;
                playProgress.setProgress(countSamplesWrittenToDisk);

                float fGain = getGainApplied();
                if (fGain == 0)
                {
                    fmt.format("0 dB");
                } else
                {
                    fmt.format("%1.1f dB", fGain);
                }
                gainApplied.setText(sbuf.toString());
                sbuf.delete(0, sbuf.length());
            }else
            {
                playProgress.setProgress(countSamplesWrittenToDisk);
            }
            recordtime.setText(getTimeStr());
        }

        int bytes = countSamplesWrittenToDisk * 2;
        int kb = bytes / 1024;
        if((bytes % 1024) > 512)++kb;

        // fmt = new Formatter(sbuf);
        if(kb < 1000)
        {
            fmt.format("%d kb", kb);
        }else
        {
            float mb = (float)kb / 1024;
            // Show mb to 1 decimal place
            fmt.format("%1.1f mb", mb);
        }
        recordkb.setText(sbuf);
    }


    private String getAudioFilePath(Context context, boolean bDeleteExisting)
    {
        File imagePath = new File(context.getExternalFilesDir(null), "data");
        if(!imagePath.exists())
        {
            boolean bDirectoryCreationStatus = imagePath.mkdirs();
            if(Debug.ON)Log.v("getAudioRecordingFilePath", "directoryCreationStatus: " + bDirectoryCreationStatus);
        }
/*
// These calls return some 34 gb on my phone which is the same as phone itself reports
        long freeSpace = imagePath.getFreeSpace();
        Log.v("getAudioRecordingFilePath", "imagePath.getFreeSpace: " + freeSpace);
        freeSpace = imagePath.getUsableSpace();
        Log.v("getAudioRecordingFilePath", "imagePath.getUsableSpace: " + freeSpace);
*/
        File newFile = new File(imagePath, "RecordedData.bin");


        File filePath = newFile.getAbsoluteFile();

        currentAudioFileLength = 0;
        if(newFile.exists())
        {
            currentAudioFileLength = (int) newFile.length();
            if(Debug.ON)Log.v("getAudioRecordingFilePath", "RecordedData.bin length: " + currentAudioFileLength);
            if(bDeleteExisting)
            {
                currentAudioFileLength = 0;
                if(newFile.delete())
                {
                    if(Debug.ON)Log.v("getAudioRecordingFilePath", "Deleted pre-existing RecordedData.bin");
                }
            }
        }
        if(Debug.ON)Log.v("getAudioRecordingFilePath", "Get absolute file path: " + filePath.getAbsoluteFile());
        return filePath.getAbsolutePath();
    }


    // @Override
    public void onAudioFocusChange(int focusChange)
    {
        switch(focusChange)
        {
            case AudioManager.AUDIOFOCUS_LOSS_TRANSIENT:
                // Pause playback because your Audio Focus was
                // temporarily stolen, but will be back soon.
                // i.e. for a phone call
                if(Debug.ON)Log.d(TAG, "AUDIOFOCUS_LOSS_TRANSIENT");

                if (ToggleOnOffState == ToggleState.ON)
                {
                    if (!bPauseToggle)
                    {
                        onTogglePause(null);
                    }
                }
                break;
            case AudioManager.AUDIOFOCUS_LOSS_TRANSIENT_CAN_DUCK:
                // Lower the volume, because something else is also
                // playing audio over you.
                // i.e. for notifications or navigation directions
                // Depending on your audio playback, you may prefer to
                // pause playback here instead.
                if(Debug.ON)Log.d(TAG, "AUDIOFOCUS_LOSS_TRANSIENT_CAN_DUCK");
                if (ToggleOnOffState == ToggleState.ON)
                {
                    if (!bPauseToggle)
                    {
                        onTogglePause(null);
                    }
                }
                break;
            case AUDIOFOCUS_GAIN:
                // Resume playback, because you hold the Audio Focus again!
                // i.e. the phone call ended or the nav directions are finished
                // If you implement ducking and lower the volume,
                // be sure to return it to normal here, as well.
                if(Debug.ON)Log.d(TAG, "AUDIOFOCUS_GAIN");
                if(ToggleOnOffState == ToggleState.ON)
                {
                    if(bPauseToggle)
                    {
                        onTogglePause(null);
                    }
                }
                break;
            case AudioManager.AUDIOFOCUS_LOSS:
                // Stop playback, because you lost the Audio Focus.
                // i.e. the user started some other playback app
                // Remember to unregister your controls/buttons here.
                // and release the kra - Audio Focus! You're done.
                // am.abandonAudioFocus(afChangeListener);
                if(Debug.ON)Log.d(TAG, "AUDIOFOCUS_LOSS");
                // I only ever get this AUDIOFOCUS_LOSS message once.
                // In attempt to fix that, tried abandoning audio focus.

                // We are supposed to abandon audio focus here, but I don't want to give it up.
                // If I abandon it, the moment I ask for it again I get AUDIOFOCUS_LOSS again.
                // Better not to give it up in the first place!
                break;
            // According to the docs, we only have the 4 types above
            default:
                if(Debug.ON)Log.d(TAG, "Unknown audio focus change mode: " + focusChange);
                break;
        } // End switch
    }

//    @SuppressLint("WakelockTimeout")
    @Override
    protected void onCreate(Bundle savedInstanceState)
    {
        super.onCreate(savedInstanceState);
        Display display = getWindowManager().getDefaultDisplay();
        Point displaySize = new Point();
        display.getSize(displaySize);
        setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_NOSENSOR);
        setContentView(R.layout.activity_main);

        ActionBar sb = getSupportActionBar();
        assert sb != null;
        // https://stackoverflow.com/questions/5861661/actionbar-text-color excellent discussion
        sb.setTitle(Html.fromHtml("<font color='#444444'>" + getString(R.string.app_name) + "</font>", Html.FROM_HTML_MODE_LEGACY));
        int bkgrndColour = ResourcesCompat.getColor(getResources(), R.color.colorTealAccent, null);
        ColorDrawable cd = new ColorDrawable(bkgrndColour);
        try {
            Objects.requireNonNull(sb).setBackgroundDrawable(cd);
        }catch (NullPointerException e)
        {
            if(Debug.ON)Log.e("Autocord", e.toString());
        }
        fPeakAmplitude = 1.f;
        curMode = INITIAL;
        ToggleOnOffState = ToggleState.OFF;
        currentAudioFileLength = 0;
        countTotalEvents = 0;
        countContiguousEventGroups = 0;
        bBeganContiguousEventsGroup = false;
        bPauseToggle = false;
        bTimerStopping = false;
        bTimerSleeping = true;
        bExitApp = false;
        bScreenIsOn = true;
        bTimerWakeUp = false;

        customCanvas = findViewById(R.id.signature_canvas);
        mbtnStart = findViewById(R.id.imageBtnStart);
        mbtnStop = findViewById(R.id.imageBtnStop);
        mbtnPause = findViewById(R.id.imageBtnPause);
        mbtnUnPause = findViewById(R.id.imageBtnUnPause);
        // ImageButton mbtnExport = findViewById(R.id.imageBtnExport);
        peakFreq = (TextView) findViewById(R.id.peakFreq);
        peakDB = (TextView) findViewById(R.id.peakDB);
        musicalNote = (TextView) findViewById(R.id.musicalNote);
        currentMode = (TextView) findViewById(R.id.labelCurrentMode);
        radioMonitor = (RadioButton) findViewById(R.id.radioMonitor);
        radioRecord = (RadioButton) findViewById(R.id.radioRecord);
        radioPlay = (RadioButton) findViewById(R.id.radioPlay);
        recordkb = (TextView) findViewById(R.id.recordkb);
        recordtime = (TextView) findViewById(R.id.recordtime);
        gainApplied = (TextView) findViewById(R.id.gainApplied);
        playProgress = findViewById(R.id.progressPlay);
        UIModeSharedPrefsOp(PREFS_ACTION.GET);
        SetUIMode(bUIModeSimplified, false);
        PrefilterPrefsOp(PREFS_ACTION.GET);
        setPrefilter(bPrefilterOn);
        InputBoostPrefsOp(PREFS_ACTION.GET);
        setInputBoost(bInputBoostOn);
        EncoderTypePrefsOp(PREFS_ACTION.GET);
        TriggerPresetPrefsOp(PREFS_ACTION.GET);

        // Screen must be a minimum of 964 pixels wide and 644 pixels high
        if((displaySize.x < FRAMEDGRAPH_WIDTH) || (displaySize.y < FRAMEDGRAPH_HEIGHT * 2))
        {
            Toast.makeText(MainActivity.this, "Sorry can't do this. Screen too small!", Toast.LENGTH_LONG).show();
            bExitApp = true;
            setTimer(3000);
            return;
        }

        // From debug output...
        // V/ContentValues: Notification channel was already registered!
        // W/Notification: Use of stream types is deprecated for operations other than volume control
        // See the documentation of setSound() for what to use instead with android.media.AudioAttributes to qualify your playback use case
        // Maybe has nothing to do with code here since line before was: V/ContentValues: Notification channel was already registered!

        audioManager = (AudioManager) getSystemService(AUDIO_SERVICE);

        AudioAttributes mPlaybackAttributes = new AudioAttributes.Builder()
                .setUsage(AudioAttributes.USAGE_MEDIA)
                .setContentType(AudioAttributes.CONTENT_TYPE_SPEECH)
                .build();
        // Jan 30/2021 - According to the docs, "The FocusGain field is required; all the other fields are optional."
        AudioFocusRequest mFocusRequest = new AudioFocusRequest.Builder(AUDIOFOCUS_GAIN)
                .setFocusGain(AUDIOFOCUS_GAIN) // Added Jan 30/2021 - made no difference that I could see
                .setAudioAttributes(mPlaybackAttributes)
                .setAcceptsDelayedFocusGain(true)
                .setWillPauseWhenDucked(true)
                .setOnAudioFocusChangeListener(this)
                .build();

        MediaPlayer mMediaPlayer = new MediaPlayer();
        mMediaPlayer.setAudioAttributes(mPlaybackAttributes);
        audioManager.requestAudioFocus(mFocusRequest);
        bluetoothManager = new BluetoothManager(audioManager);

/*
        int res = audioManager.requestAudioFocus(mFocusRequest);
        final Object mFocusLock = new Object();
        synchronized (mFocusLock)
        {
            if (res == AudioManager.AUDIOFOCUS_REQUEST_FAILED)
            {
                // mPlaybackDelayed = false;
                Log.e("requestAudioFocus", "AUDIOFOCUS_REQUEST_FAILED");
            } else if (res == AudioManager.AUDIOFOCUS_REQUEST_GRANTED)
            {
                // mPlaybackDelayed = false;
                Log.e("requestAudioFocus", "AUDIOFOCUS_REQUEST_GRANTED");
            } else if (res == AudioManager.AUDIOFOCUS_REQUEST_DELAYED)
            {
                // mPlaybackDelayed = true;
                // playbackNow();
                Log.e("requestAudioFocus", "AUDIOFOCUS_REQUEST_DELAYED");
            }
        }
 */
        mMediaPlayer.release();


// Don't need this and it puts up a screen - Activity Action: Show Notification listener settings.
       // Intent i=new Intent("android.settings.ACTION_NOTIFICATION_LISTENER_SETTINGS");
       // startActivity(i);
        broadcastrcvr = new BroadcastRcvr();

        IntentFilter filter = new IntentFilter(ConnectivityManager.CONNECTIVITY_ACTION);
        filter.addAction(Intent.ACTION_SCREEN_ON);
        filter.addAction(Intent.ACTION_SCREEN_OFF);
        this.registerReceiver(broadcastrcvr, filter);

        // To keep screen on... https://developer.android.com/training/scheduling/wakelock
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        // if you want to explicitly clear the flag and thereby allow the screen to turn off again,
        // use clearFlags(): getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON).

        // To keep the cpu on do the following (besides the permission request in AndroidManifest) ...
        // Wake lock level: Ensures that the CPU is running; the screen and keyboard backlight will be allowed to go off.
        // PARTIAL_WAKE_LOCK flag: If the user presses the power button, then the screen will be turned off
        // but the CPU will be kept on until all partial wake locks have been released.
        PowerManager powerManager = (PowerManager) getSystemService(POWER_SERVICE);
        assert powerManager != null;
        wakeLock = powerManager.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK,
                "Autocord::MyWakelockTag");
        wakeLock.acquire(); // Not using a timeout value

        Intent serviceIntent = new Intent(this, AudioService.class);
        // serviceIntent.putExtra("Mode", INITIAL);
        // serviceIntent.setAction(SyncStateContract.Constants.ACTION.MAIN_ACTION);
        // serviceIntent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_CLEAR_TASK);
        startForegroundService(serviceIntent);
        bindService(serviceIntent, (ServiceConnection) this, BIND_AUTO_CREATE);
        setMode(INITIAL); // Initialize AudioEngine
        logFileAccess = new LogFileAccess();
        audioStorage = new AudioStorage(getApplicationContext());
    }

    private void SettingsBtnCtrl(boolean bEnable)
    {
       ImageButton BtnSettings = findViewById(R.id.imageBtnPopup);
       if(bEnable)
       {
           findViewById(R.id.imageBtnInfo).setVisibility(View.INVISIBLE);
           BtnSettings.setAlpha(1.0f);
           BtnSettings.setEnabled(true);
       }else
       {
           BtnSettings.setAlpha(0.5f);
           BtnSettings.setEnabled(false);
       }
    }
    private void StartBtnCtrl(boolean bEnable)
    {
        ImageButton BtnStart = findViewById(R.id.imageBtnStart);
        if(bEnable)
        {
            BtnStart.setAlpha(1.0f);
            BtnStart.setEnabled(true);
        }else
        {
            BtnStart.setAlpha(0.5f);
            BtnStart.setEnabled(false);
        }
    }
    private void PauseBtnCtrl(boolean bEnable)
    {
        ImageButton BtnPause = findViewById(R.id.imageBtnPause);
        if(bEnable)
        {
            BtnPause.setAlpha(1.0f);
            BtnPause.setEnabled(true);
        }else
        {
            BtnPause.setAlpha(0.5f);
            BtnPause.setEnabled(false);
        }
    }
    private void ExportBtnCtrl(boolean bEnable)
    {
        ImageButton BtnExport = findViewById(R.id.imageBtnExport);
        if(bEnable)
        {
            BtnExport.setAlpha(1.0f);
            BtnExport.setEnabled(true);
        }else
        {
            BtnExport.setAlpha(0.5f);
            BtnExport.setEnabled(false);
        }
    }

    public void onUIModeRadioButtonClicked(View view)
    {
        // Is the button now checked?
        boolean checked = ((RadioButton) view).isChecked();
        switch(view.getId()) {
            case R.id.radioSimplified:
                if (checked)
                {
                    // RadioButton rb = findViewById(R.id.radioAdvanced);
                    // rb.setChecked(false);
                    bUIModeSimplified = true;
                }
                if(Debug.ON)Log.v(TAG, "set UI mode simplified");
                break;
            case R.id.radioAdvanced:
                if (checked)
                {
                    // RadioButton rb = findViewById(R.id.radioSimplified);
                    // rb.setChecked(false);
                    bUIModeSimplified = false;
                }
                if(Debug.ON)Log.v(TAG, "set UI mode advanced");
                break;
        }
        SetUIMode(bUIModeSimplified, true);
        if (bUIModeSimplified)
        {
            mOtionsView.findViewById(R.id.checkbox_prefilter).setVisibility(View.INVISIBLE);
            mOtionsView.findViewById(R.id.checkbox_inputboost).setVisibility(View.INVISIBLE);
            mOtionsView.findViewById(R.id.radioAAC64k).setVisibility(View.INVISIBLE);
            mOtionsView.findViewById(R.id.radioAAC128k).setVisibility(View.INVISIBLE);
            mOtionsView.findViewById(R.id.radioFLAC).setVisibility(View.INVISIBLE);
            mOtionsView.findViewById(R.id.radioVoice).setVisibility(View.INVISIBLE);
            mOtionsView.findViewById(R.id.radioBirds).setVisibility(View.INVISIBLE);
            mOtionsView.findViewById(R.id.radioAny).setVisibility(View.INVISIBLE);
        }else
        {
            mOtionsView.findViewById(R.id.checkbox_prefilter).setVisibility(View.VISIBLE);
            mOtionsView.findViewById(R.id.checkbox_inputboost).setVisibility(View.VISIBLE);
            mOtionsView.findViewById(R.id.radioAAC64k).setVisibility(View.VISIBLE);
            mOtionsView.findViewById(R.id.radioAAC128k).setVisibility(View.VISIBLE);
            mOtionsView.findViewById(R.id.radioFLAC).setVisibility(View.VISIBLE);
            mOtionsView.findViewById(R.id.radioVoice).setVisibility(View.VISIBLE);
            mOtionsView.findViewById(R.id.radioBirds).setVisibility(View.VISIBLE);
            mOtionsView.findViewById(R.id.radioAny).setVisibility(View.VISIBLE);
        }

        UIModeSharedPrefsOp(PREFS_ACTION.SET);
    }

    public void onOptionPrefilterClicked(View view)
    {
        bPrefilterOn = ((CheckBox) view).isChecked();
        setPrefilter(bPrefilterOn);
        PrefilterPrefsOp(PREFS_ACTION.SET);
    }

    public void onOptionInputBoostClicked(View view)
    {
        bInputBoostOn = ((CheckBox) view).isChecked();
        setInputBoost(bInputBoostOn);
        InputBoostPrefsOp(PREFS_ACTION.SET);
    }

    public void onModeRadioButtonClicked(View view) {
        // Is the button now checked?
        boolean checked = ((RadioButton) view).isChecked();
        customCanvas.ClearScreen();
        // Check which radio button was clicked
        switch(view.getId()) {
            case R.id.radioMonitor:
                if (checked)
                {
                    curMode = MONITOR;
                    setMode(MONITOR);
                    SettingsBtnCtrl(true);
                    radioRecord.setChecked(false);
                    radioPlay.setChecked(false);
                    currentMode.setText(getResources().getString(R.string.monitoring));
                    TextView label = findViewById(R.id.labelGainApplied);
                    label.setText(getResources().getString(R.string.inputlevel));
                    ShowRecordFileStats(0); // Reset display in case was showing previously
                }
                if(Debug.ON)Log.v(TAG, "set mode MONITOR");
                    break;
            case R.id.radioRecord:
                if (checked)
                {
                    curMode = RECORD;
                    setMode(RECORD);
                    TextView label = findViewById(R.id.labelRecordtime);
                    label.setText(getResources().getString(R.string.duration));
                    label = findViewById(R.id.labelGainApplied);
                    label.setText(getResources().getString(R.string.inputlevel));
                    SettingsBtnCtrl(true);
                    radioMonitor.setChecked(false);
                    radioPlay.setChecked(false);
                    currentMode.setText(getResources().getString(R.string.recording));
                    ShowRecordFileStats(0); // Reset display in case was showing previously
                }
                if(Debug.ON)Log.v(TAG, "set mode RECORD");
                    break;
            case R.id.radioPlay:
                if (checked)
                {
                    curMode = PLAY;
                    setMode(PLAY);
                    TextView label = findViewById(R.id.labelRecordtime);
                    label.setText(getResources().getString(R.string.capture_time));
                    label = findViewById(R.id.labelGainApplied);
                    label.setText(getResources().getString(R.string.gain_applied));
                    SettingsBtnCtrl(false);
                    customCanvas.ResetTriggers();
                    radioMonitor.setChecked(false);
                    radioRecord.setChecked(false);
                    currentMode.setText(getResources().getString(R.string.playing));
                    // long began = System.nanoTime();
                    getAudioFilePath(getApplicationContext(), false);
                    // Log.d(TAG,"getAudioFilePath(): " + (float)(System.nanoTime() - began) / 1000000);
                    if(currentAudioFileLength >= 32768)
                    {
                        // AudioInfoSharedPrefsOp(PREFS_ACTION.GET);  // To get fPeakAmplitude, contiguousEventGroups, and TotalEvents
                        // bShowLogHeader = true;
                        findViewById(R.id.imageBtnInfo).setVisibility(View.VISIBLE);
                    }
                }
                if(Debug.ON)Log.v(TAG, "set mode PLAY");
                    break;
        }
        // if(bShowLogHeader)ShowLogfileHeader();
        // String[] sValues = logFileAccess.GetEvent(); for testing only
    }

    private void RadioButtonsEnable(boolean bEnable)
    {
        radioMonitor.setEnabled(bEnable);
        radioRecord.setEnabled(bEnable);
        radioPlay.setEnabled(bEnable);
        if(bEnable)
        {
            radioMonitor.setVisibility(View.VISIBLE);
            radioRecord.setVisibility(View.VISIBLE);
            radioPlay.setVisibility(View.VISIBLE);
        }else
        {
            radioMonitor.setVisibility(View.INVISIBLE);
            radioRecord.setVisibility(View.INVISIBLE);
            radioPlay.setVisibility(View.INVISIBLE);
        }
    }

    public void onClickBtnToggle(View view)
    {
        if(checkExpired())
        {
            Toast.makeText(MainActivity.this, "This license for this app has expired!", Toast.LENGTH_LONG).show();
            return;
        }
        if(ToggleOnOffState == ToggleState.ON)
        {
            ToggleOnOffState = ToggleState.OFF;
            if(Debug.ON)Log.v(TAG, "set toggle state OFF");
        }else
        {
            ToggleOnOffState = ToggleState.ON;
            if(Debug.ON)Log.v(TAG, "set toggle state ON");
        }

        if(ToggleOnOffState == ToggleState.ON)
        {
            ExportBtnCtrl(false);
            if (curMode == INITIAL)
            {
                // User didn't set the mode. Set the default...
                curMode = MONITOR;
                setMode(MONITOR);
            }

            if ((curMode == MONITOR) || (curMode == RECORD))
            {
                if (view != null)
                {
                    if (!isRecordPermissionGranted())
                    {
                        // See https://developer.android.com/training/permissions/requesting
                        ToggleOnOffState = ToggleState.OFF;
                        requestRecordPermission();
                        return;
                        // This function will be called again with view = null
                        // upon permission granted from onRequestPermissionsResult()
                    }
                }

                if (curMode == RECORD)
                {
                    countContiguousEventGroups = 0;
                    countTotalEvents = 0;
                    bBeganContiguousEventsGroup = false;
                    logFileAccess.Close();
                }

                // To show as VU meter...
                playProgress.setProgress(0);
                playProgress.setMax(70);
                int colour = ResourcesCompat.getColor(getResources(), R.color.colorVUMeter, null); // Light green
                playProgress.setProgressTintList(ColorStateList.valueOf(colour));

            }

            if (curMode != MONITOR)
            {
                String pathname = getAudioFilePath(getApplicationContext(), (curMode == RECORD));
                // long began = System.nanoTime();
                doFileIO(pathname, currentAudioFileLength);
                // Log.d(TAG,"doFileIO(): " + (float)(System.nanoTime() - began) / 1000000);
            }

            if (Debug.ON) Log.v(TAG, "currentAudioFileLength: " + currentAudioFileLength);

            customCanvas.ClearBitmap(); // New - moved to here

            if (curMode == PLAY)
            {
                if (currentAudioFileLength < 32768)
                {
                    Toast.makeText(MainActivity.this, "There is no audio data to play!", Toast.LENGTH_LONG).show();
                    ToggleOnOffState = ToggleState.OFF;
                    return;
                }
                playProgress.setProgress(0);
                playProgress.setMax(currentAudioFileLength / 2);

                StringBuilder sbuf = new StringBuilder();
                Formatter fmt = new Formatter(sbuf);
                float fGain = getGainApplied();
                if (fGain == 0)
                {
                    fmt.format("0 dB");
                } else
                {
                    fmt.format("%1.1f dB", fGain);
                }
                gainApplied.setText(sbuf.toString());
                startEngine(fPeakAmplitude);

            } else
            {
                startEngine(bluetoothManager.GetDeviceID());
            }
            // long began = System.nanoTime();

            // customCanvas.ClearBitmap(); was here
            // startEngine(fPeakAmplitude); was here

            // Log.d(TAG,"startEngine(): " + (float)(System.nanoTime() - began) / 1000000);
            mbtnStop.setVisibility(View.VISIBLE);
            mbtnStart.setVisibility(View.INVISIBLE);
            RadioButtonsEnable(false);
            setTimer(100);
        }else
        {
            setTimer(STOP_TIMER);
            float fTmpPeakAmplitude = stopEngine();
            if(bPauseToggle)
            {
                onTogglePause(null);
            }
            bluetoothManager.CloseConnection();

            playProgress.setProgress(0); // clear VU meter
            int colour = ResourcesCompat.getColor(getResources(), R.color.colorProgress, null); // Light green
            playProgress.setProgressTintList(ColorStateList.valueOf(colour));

            mbtnStart.setVisibility(View.VISIBLE);
            mbtnStop.setVisibility(View.INVISIBLE);
            RadioButtonsEnable(true);
            if(curMode == RECORD)
            {
                countTotalEvents = getTotalEventsWrittenToLog();
                if(Debug.ON)Log.v(TAG, "countContiguousSamplesGroups: " + countContiguousEventGroups);
                if(Debug.ON)Log.v(TAG, "Total events written to log: " + countTotalEvents);
                fPeakAmplitude = fTmpPeakAmplitude;
                AudioInfoSharedPrefsOp(PREFS_ACTION.SET);  // To save fPeakAmplitude, countContiguousEventGroups, countTotalEvents

               // Maybe we left the recording lamp lit. Ensure it is extinguished
                currentMode.setTextColor(ResourcesCompat.getColor(getResources(), R.color.colorTealAccent2, null));
                if(view != null) // view = null if onClickBtnToggle() called from code
                {
                    customCanvas.Redraw();
                }
            }else if(curMode == PLAY)
            {
                // playProgress.setProgress(0); was here - now moved up to clear VU meter
                gainApplied.setText(getResources().getString(R.string._0_db));
                if(view != null) // view = null if onClickBtnToggle() called from code
                {
                    if(Debug.ON)Log.v(TAG, "Calling Rewind on stop Play");
                    logFileAccess.Rewind(); // In case we start Play again
                }
            }
            ExportBtnCtrl(true);
        }
        if(Debug.ON)Log.v(TAG, "exit onClickBtnToggle");
    }

    void setTimer(int delayMillis)
    {
        bTimerStopping = true;
        bTimerWakeUp = false;
        bTimerSleeping = false;
        // sleep(2);
        mTimerHandler.removeCallbacks(mTimerRunnable);
        if(delayMillis != STOP_TIMER)
        {
            // if (Debug.ON)Log.v(TAG, "Set timer!");
            bTimerStopping = false;
            bTimerWakeUp = false;
            bTimerSleeping = true;
            mTimerHandler.postDelayed(mTimerRunnable, delayMillis);
            // Log.v(TAG, "mTimerHandler.postDelayed");
        }else
        {
            if(Debug.ON)Log.v(TAG, "Killed timer!");
        }
    }

    // Called when screen loses focus, like by pressing the home button,
    // but not called when we are quitting via pressing the back button
    @Override
    public void onPause()
    {
        super.onPause();
/*
        if(player != null)
        {
            player.stop();
            player = null;
        }

 */
        // This has been fixed by a line in the manifest
        // We need to check if settings dialog is showing and kill it
        // Otherwise it will remain on the screen without the app
        // and when we bring the app back we only see the dialog
        // and no app screen. It's ugly. However, we can't detect
        // that here because according to documentation https://stackoverflow.com/questions/7240916/android-under-what-circumstances-would-a-dialog-appearing-cause-onpause-to-be/7384782#7384782
        // we get onPause() when our dialog activity is popped up.
        // See https://stackoverflow.com/questions/11026234/how-to-check-if-the-current-activity-has-a-dialog-in-front
        if(Debug.ON)Log.v(TAG, "Paused MainActivity!");
    }

    /** Called when the activity is about to be destroyed. */
    @Override
    protected void onDestroy()
    {
        if(Debug.ON)Log.v(TAG, "Destroying...");
        if(ToggleOnOffState == ToggleState.ON)
        {
            onClickBtnToggle(null);
        }
        // audioManager.abandonAudioFocus(afChangeListener);
        // Log.v(TAG, "abandonAudioFocus");

        this.unregisterReceiver(broadcastrcvr);
        if(Debug.ON)Log.v(TAG, "unregisterReceiver");
        wakeLock.release();
        if(Debug.ON)Log.v(TAG, "Released wakelock!");
        setMode(DESTROY);
        if(Debug.ON)Log.v(TAG, "Destroyed AudioEngine!");
        logFileAccess.Close();
        if(Debug.ON)Log.v(TAG, "Closed logFileAccess!");
        if (audioService != null)
        {
            audioService.setCallBack(null);
            unbindService(this);
            if(Debug.ON)Log.v(TAG, "Unbound service!");
        }
        Intent serviceIntent = new Intent(this, AudioService.class);
        // serviceIntent.putExtra("Mode", DESTROY);
        stopService(serviceIntent);
        if(Debug.ON)Log.v(TAG, "Stopped service!");

        super.onDestroy();
    }

    // Called from AudioEngine
    // see void AudioEngine::signalDataReady(int dataExported)
    public static void dataReady(int dataExported)
    {
        dataWritten = dataExported;
        bTimerWakeUp = true;
        // Log.v("dataReady", "dataWritten: " + dataWritten);
    }

    public void onTriggerRadioButtonClicked(View view)
    {
        boolean checked = ((RadioButton) view).isChecked();

        // Check which radio button was clicked
        switch(view.getId()) {
            case R.id.radioVoice:
                if (checked)
                {
                    triggerPreset = PopSettings.PRESETS.VOICE;
                    TriggerPresetPrefsOp(PREFS_ACTION.SET);
                }
                if(Debug.ON)Log.v(TAG, "Selected trigger preset VOICE");
                break;
            case R.id.radioBirds:
                if (checked)
                {
                    triggerPreset = PopSettings.PRESETS.BIRDS;
                    TriggerPresetPrefsOp(PREFS_ACTION.SET);
                }
                if(Debug.ON)Log.v(TAG, "Selected trigger preset BIRDS");
                break;
            case R.id.radioAny:
                if (checked)
                {
                    triggerPreset = PopSettings.PRESETS.ANY;
                    TriggerPresetPrefsOp(PREFS_ACTION.SET);
                }
                if(Debug.ON)Log.v(TAG, "Selected  trigger preset ANY");
                break;
        }

    }

    public void onEncoderRadioButtonClicked(View view)
    {
        boolean checked = ((RadioButton) view).isChecked();

        // Check which radio button was clicked
        switch(view.getId()) {
            case R.id.radioAAC64k:
                if (checked)
                {
                    encoderType = AudioEncoder.ENCODER_TYPE.AAC;
                    encoderBitRate = 64000;
                    EncoderTypePrefsOp(PREFS_ACTION.SET);
                }
                if(Debug.ON)Log.v(TAG, "Selected encoder AAC64k");
                break;
            case R.id.radioAAC128k:
                if (checked)
                {
                    encoderType = AudioEncoder.ENCODER_TYPE.AAC;
                    encoderBitRate = 128000;
                    EncoderTypePrefsOp(PREFS_ACTION.SET);
                }
                if(Debug.ON)Log.v(TAG, "Selected encoder AAC128k");
                break;
            case R.id.radioFLAC:
                if (checked)
                {
                    encoderType = AudioEncoder.ENCODER_TYPE.FLAC;
                    EncoderTypePrefsOp(PREFS_ACTION.SET);
                }
                if(Debug.ON)Log.v(TAG, "Selected encoder FLAC");
                break;
        }

    }

    public void StartEncoder(String flacFileName, String pathRecordedData)
    {
        if(Debug.ON)Log.v("StartEncoder", "inside StartEncoder!");
        String mediaPathName = audioStorage.GetExternalMediaPath(flacFileName);
        AudioEncoder audioEncoder = new AudioEncoder(mediaPathName, currentAudioFileLength / 2, encoderType, encoderBitRate);
        if(!audioEncoder.createCodec())
        {
            Toast.makeText(MainActivity.this, "Sorry, can't do this - codec error!", Toast.LENGTH_LONG).show();
            return;
        }
        savedMode = curMode;
        curMode = EXPORT;
        setMode(EXPORT);

        passMediaPathname(mediaPathName); // Passes pathname used for exported log
        passEncodeBuffer(AudioEncoder.encoderInputBuffer);
        doFileIO(pathRecordedData, currentAudioFileLength);
        playProgress.setProgress(0);
        playProgress.setMax(currentAudioFileLength / 2);
        customCanvas.ClearBitmap();

        savedCurrentModeText = (String) currentMode.getText();
        currentMode.setText(getResources().getString(R.string.exporting));
        RadioButtonsEnable(false);
        SettingsBtnCtrl(false);
        StartBtnCtrl(false);
        PauseBtnCtrl(false);
        startEngine(fPeakAmplitude); // This doesn't really start the engine - just passes fPeakAmplitude
        setTimer(100);
        audioEncoder.RunEncoder();
    }

    public void UpdateEncoderProgress(int dataWritten)
    {
        boolean bError = false;
        if(dataWritten == -1)
        {
            dataWritten = currentAudioFileLength / 2;
            bError = true;
            Toast.makeText(MainActivity.this, "Sorry, can't do this - codec error!", Toast.LENGTH_LONG).show();
        }else
        {
            playProgress.setProgress(dataWritten);
        }
        if(Debug.ON)Log.v("UpdateEncoderProgress", "dataWritten: " + dataWritten);
        if((dataWritten * 2) == currentAudioFileLength)
        {
            bTimerStopping = true;
            stopEngine();
            RadioButtonsEnable(true);
            currentMode.setText(savedCurrentModeText);
            SettingsBtnCtrl(true);
            StartBtnCtrl(true);
            PauseBtnCtrl(true);
            playProgress.setProgress(0);
            bTimerStopping = true;

            curMode = savedMode;
            if(curMode != INITIAL)
            {
                setMode(curMode);
            }

            if (Debug.ON) Log.v("Run loop", "Done file export!");
            // We have no idea if it was successful
            // Maybe one day I should write code to verify it and then present this toast
            // Toast.makeText(MainActivity.this, "Success!", Toast.LENGTH_LONG).show();
            if(bError)
            {
                String pathname = logFileAccess.GetDateTime(this.getApplicationContext(), encoderType); //  like "20200908151016.flac"

                int indexExtension = pathname.lastIndexOf('.');
                pathname = pathname.substring(0, indexExtension + 1);
                pathname = pathname.concat("txt");
                String mediaPathName = audioStorage.GetExternalMediaPath(pathname);
                Log.v("UpdateEncoderProgress logfile pathname", mediaPathName);

                File outfile = new File(mediaPathName);
                if (outfile.exists())
                {
                    if (outfile.delete())
                    {
                        Log.v("UpdateEncoderProgress", "Deleted text file!");
                    }
                }
            }

        }
    }

    private void requestWriteExternalStoragePermission()
    {
        ActivityCompat.requestPermissions(
                this,
                new String[]{Manifest.permission.WRITE_EXTERNAL_STORAGE},
                AUTOCORD_REQUEST);
    }

    private boolean isWriteExternalStoragePermissionGranted()
    {
        return (ActivityCompat.checkSelfPermission(this, Manifest.permission.WRITE_EXTERNAL_STORAGE) ==
                PERMISSION_GRANTED);
    }
/*
    private void requestReadExternalStoragePermission()
    {
        ActivityCompat.requestPermissions(
                this,
                new String[]{Manifest.permission.READ_EXTERNAL_STORAGE},
                AUTOCORD_REQUEST);
    }

    private boolean isReadExternalStoragePermissionGranted()
    {
        return (ActivityCompat.checkSelfPermission(this, Manifest.permission.READ_EXTERNAL_STORAGE) ==
                PERMISSION_GRANTED);
    }
*/

    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions, @NonNull int[] grantResults)
    {
        // Check that our permission was granted
        if (permissions.length > 0 &&
                permissions[0].equals(Manifest.permission.RECORD_AUDIO) &&
                grantResults[0] == PERMISSION_GRANTED)
        {
            if(Debug.ON)Log.v(TAG, "Recording permission granted!");
            onClickBtnToggle(null);
        }
        if (permissions.length > 0 &&
                permissions[0].equals(Manifest.permission.READ_EXTERNAL_STORAGE) &&
                grantResults[0] == PERMISSION_GRANTED)
        {
            if(Debug.ON)Log.v(TAG, "READ_EXTERNAL_STORAGE permission granted!");
        }
        if (permissions.length > 0 &&
                permissions[0].equals(Manifest.permission.WRITE_EXTERNAL_STORAGE) &&
                grantResults[0] == PERMISSION_GRANTED)
        {
            if(Debug.ON)Log.v(TAG, "WRITE_EXTERNAL_STORAGE permission granted!");
            onExportFile(null);
        }

        if (permissions.length == 0)
        {
            if(Debug.ON)Log.v(TAG, "permissions.length == 0!");
        }else if(grantResults[0] == PERMISSION_DENIED)
        {
            if(permissions[0].equals(Manifest.permission.WRITE_EXTERNAL_STORAGE))
            {
                if(Debug.ON)Log.v(TAG, "WRITE_EXTERNAL_STORAGE permission denied!");
            }else if(permissions[0].equals(Manifest.permission.READ_EXTERNAL_STORAGE))
            {
                if(Debug.ON)Log.v(TAG, "READ_EXTERNAL_STORAGE permission denied!");
            }
        }
    }

    private void requestRecordPermission()
    {
        ActivityCompat.requestPermissions(
                this,
                new String[]{Manifest.permission.RECORD_AUDIO},
                AUTOCORD_REQUEST);
    }

    private boolean isRecordPermissionGranted()
    {
        return (ActivityCompat.checkSelfPermission(this, Manifest.permission.RECORD_AUDIO) ==
                PERMISSION_GRANTED);
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu)
    {
        MenuInflater inflater = getMenuInflater();
        inflater.inflate(R.menu.main_menu, menu);
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item)
    {
        // Handle action bar item clicks here.
        int id = item.getItemId();

        if(id == R.id.options_btn)
        {
            showOptionsDialog();
        }

        if (id == R.id.help_btn) {
            showHelpDialog();
            return true;
        }
        if (id == R.id.about_btn) {
            showAboutDialog();
            return true;
        }

        return super.onOptionsItemSelected(item);
    }

    private void showHelpTextDialog() {
        AlertDialog.Builder builder = new AlertDialog.Builder(MainActivity.this);
        View mView = LayoutInflater.from(MainActivity.this).inflate(R.layout.dialog_helptext, null);

        builder.setView(mView)
                .setPositiveButton(getResources().getString(R.string.close), (dialog, id) -> dialog.cancel());

        AlertDialog alertDialog = builder.create();
        alertDialog.show();

        Resources res = getResources();

        // TextView webView = (TextView) mView.findViewById(R.id.txtHelp);
        WebView webView = mView.findViewById(R.id.webview);
        if(webView != null)
        {
            // webView.setText(fromHtml(sContent, Html.FROM_HTML_MODE_LEGACY));
            // webView.setWebViewClient(new MyBrowser());
            webView.setWebViewClient(new WebViewClient());
            webView.getSettings().setLoadsImagesAutomatically(true);
            webView.getSettings().setBuiltInZoomControls(true);
            webView.getSettings().setDisplayZoomControls(false);
            // webView.getSettings().setJavaScriptEnabled(true);
            webView.setScrollBarStyle(View.SCROLLBARS_INSIDE_OVERLAY);
            //webView.loadData(sContent, "text/html; charset=UTF-8", null);
            webView.loadUrl("file:///android_asset/Autocord4AndroidHelp.html");
        }

        int bkgrndColour = ResourcesCompat.getColor(res, R.color.colorTealPrimary, null);
        int textColour = ResourcesCompat.getColor(res, R.color.colorTealAccent2, null);

        Button btn = alertDialog.getButton(AlertDialog.BUTTON_POSITIVE);
        btn.setCompoundDrawablesWithIntrinsicBounds(R.drawable.ic_close, 0, 0, 0);
        btn.setTextColor(textColour);

        ColorDrawable cd = new ColorDrawable(bkgrndColour);

        try {
            Objects.requireNonNull(alertDialog.getWindow()).setBackgroundDrawable(cd);
        }catch (java.lang.NullPointerException e)
        {
            if(Debug.ON)Log.e("Autocord", e.toString());
        }
    }

    private void showHelpDialog() {
        AlertDialog.Builder builder = new AlertDialog.Builder(MainActivity.this);
        View mView = LayoutInflater.from(MainActivity.this).inflate(R.layout.dialog_help, null);

        builder.setView(mView)
                .setPositiveButton(getResources().getString(R.string.close), (dialog, id) -> dialog.cancel());

        AlertDialog alertDialog = builder.create();
        alertDialog.show();

        Resources res = getResources();
        int bkgrndColour = ResourcesCompat.getColor(res, R.color.colorTealPrimary, null);
        int textColour = ResourcesCompat.getColor(res, R.color.colorTealAccent2, null);

        Button btn = alertDialog.getButton(AlertDialog.BUTTON_POSITIVE);
        btn.setCompoundDrawablesWithIntrinsicBounds(R.drawable.ic_close, 0, 0, 0);
        btn.setTextColor(textColour);

        ColorDrawable cd = new ColorDrawable(bkgrndColour);

        try {
            Objects.requireNonNull(alertDialog.getWindow()).setBackgroundDrawable(cd);
        }catch (java.lang.NullPointerException e)
        {
            if(Debug.ON)Log.e("Autocord", e.toString());
        }
    }

    private void showAboutDialog() {
        AlertDialog.Builder builder = new AlertDialog.Builder(MainActivity.this);
        View mView = LayoutInflater.from(MainActivity.this).inflate(R.layout.dialog_about, null);
        String versionName = BuildConfig.VERSION_NAME;
        final TextView tvText = mView.findViewById(R.id.view_about1);
        tvText.setText(getResources().getString(R.string.appnameversion) + versionName);

        builder.setView(mView);

        // Add action buttons
        builder.setPositiveButton(getResources().getString(R.string.close),
                (dialog, id) -> dialog.cancel());

        builder.setNegativeButton(getResources().getString(R.string.email),
                (dialog, id) -> {
                    Intent email = new Intent(Intent.ACTION_SEND);
                    email.putExtra(Intent.EXTRA_EMAIL, new String[]{"info@TropicalCoder.com"});
                    CharSequence cs = "Re: Autocord";
                    email.putExtra(Intent.EXTRA_SUBJECT, cs);
                    email.setType("message/rfc822");
                    startActivity(email);
                    dialog.cancel();
                });

        AlertDialog alertDialog = builder.create();
        alertDialog.show();

        Resources res = getResources();
        int bkgrndColour = ResourcesCompat.getColor(res, R.color.colorTealPrimary, null);
        int textColour = ResourcesCompat.getColor(res, R.color.colorTealAccent2, null);

        Button btn = alertDialog.getButton(AlertDialog.BUTTON_POSITIVE);
        btn.setCompoundDrawablesWithIntrinsicBounds(R.drawable.ic_close, 0, 0, 0);
        btn.setTextColor(textColour);
        btn = alertDialog.getButton(AlertDialog.BUTTON_NEGATIVE);
        btn.setCompoundDrawablesWithIntrinsicBounds(R.drawable.ic_send, 0, 0, 0);
        btn.setTextColor(textColour);

        ColorDrawable cd = new ColorDrawable(bkgrndColour);

        try {
            Objects.requireNonNull(alertDialog.getWindow()).setBackgroundDrawable(cd);
        }catch (java.lang.NullPointerException e)
        {
            if(Debug.ON)Log.e("Autocord", e.toString());
        }
    }

    public void onOpenHelpDoc(View view)
    {
        if(Debug.ON)Log.v(TAG, "Opening help doc");
        showHelpTextDialog();
    }


    private void showOptionsDialog() {
        AlertDialog.Builder builder = new AlertDialog.Builder(MainActivity.this);
        View mView = LayoutInflater.from(MainActivity.this).inflate(R.layout.dialog_options, null);
        mOtionsView = mView;
        if (bUIModeSimplified)
        {
            RadioButton rb = mView.findViewById(R.id.radioSimplified);
            rb.setChecked(true);
        }else
        {
            RadioButton rb = mView.findViewById(R.id.radioAdvanced);
            rb.setChecked(true);
        }

        CheckBox cb = mView.findViewById(R.id.checkbox_prefilter);
        cb.setChecked(bPrefilterOn);

        cb = mView.findViewById(R.id.checkbox_inputboost);
        cb.setChecked(bInputBoostOn);

        RadioButton rb;
        if(encoderType == AudioEncoder.ENCODER_TYPE.AAC)
        {
            if(encoderBitRate == 64000)
            {
                rb = mView.findViewById(R.id.radioAAC64k);
                rb.setChecked(true);
            }else if(encoderBitRate == 128000)
            {
                rb = mView.findViewById(R.id.radioAAC128k);
                rb.setChecked(true);
            }
        }else if(encoderType == AudioEncoder.ENCODER_TYPE.FLAC)
        {
            rb = mView.findViewById(R.id.radioFLAC);
            rb.setChecked(true);
        }

        if(triggerPreset == PopSettings.PRESETS.VOICE)
        {
            rb = mView.findViewById(R.id.radioVoice);
            rb.setChecked(true);
        }else if(triggerPreset == PopSettings.PRESETS.BIRDS)
        {
            rb = mView.findViewById(R.id.radioBirds);
            rb.setChecked(true);
        }else if(triggerPreset == PopSettings.PRESETS.ANY)
        {
            rb = mView.findViewById(R.id.radioAny);
            rb.setChecked(true);
        }

        if (bUIModeSimplified)
        {
            mView.findViewById(R.id.checkbox_prefilter).setVisibility(View.INVISIBLE);
            mView.findViewById(R.id.checkbox_inputboost).setVisibility(View.INVISIBLE);
            mView.findViewById(R.id.radioAAC64k).setVisibility(View.INVISIBLE);
            mView.findViewById(R.id.radioAAC128k).setVisibility(View.INVISIBLE);
            mView.findViewById(R.id.radioFLAC).setVisibility(View.INVISIBLE);
            mView.findViewById(R.id.radioVoice).setVisibility(View.INVISIBLE);
            mView.findViewById(R.id.radioBirds).setVisibility(View.INVISIBLE);
            mView.findViewById(R.id.radioAny).setVisibility(View.INVISIBLE);

        }else
        {
            mView.findViewById(R.id.checkbox_prefilter).setVisibility(View.VISIBLE);
            mView.findViewById(R.id.checkbox_inputboost).setVisibility(View.VISIBLE);
            mView.findViewById(R.id.radioAAC64k).setVisibility(View.VISIBLE);
            mView.findViewById(R.id.radioAAC128k).setVisibility(View.VISIBLE);
            mView.findViewById(R.id.radioFLAC).setVisibility(View.VISIBLE);
            mView.findViewById(R.id.radioVoice).setVisibility(View.VISIBLE);
            mView.findViewById(R.id.radioBirds).setVisibility(View.VISIBLE);
            mView.findViewById(R.id.radioAny).setVisibility(View.VISIBLE);
        }

        builder.setView(mView)
                .setPositiveButton(getResources().getString(R.string.close), (dialog, id) -> dialog.cancel());
        AlertDialog alertDialog = builder.create();
        alertDialog.show();


        Resources res = getResources();
        int bkgrndColour = ResourcesCompat.getColor(res, R.color.colorTealPrimary, null);
        int textColour = ResourcesCompat.getColor(res, R.color.colorTealAccent2, null);

        Button btn = alertDialog.getButton(AlertDialog.BUTTON_POSITIVE);
        btn.setCompoundDrawablesWithIntrinsicBounds(R.drawable.ic_close, 0, 0, 0);
        btn.setTextColor(textColour);

        ColorDrawable cd = new ColorDrawable(bkgrndColour);

        try {
            Objects.requireNonNull(alertDialog.getWindow()).setBackgroundDrawable(cd);
        }catch (java.lang.NullPointerException e)
        {
            if(Debug.ON)Log.e("Autocord", e.toString());
        }
    }

    private void showExportDialog(String pathRecordedData)
    {
        String flacFileName = logFileAccess.GetDateTime(this.getApplicationContext(), encoderType); //  like "20200908151016.flac"

        AlertDialog.Builder builder = new AlertDialog.Builder(MainActivity.this);
        View mView = LayoutInflater.from(MainActivity.this).inflate(R.layout.dialog_export, null);

        final TextView tvText = mView.findViewById(R.id.view_export2);
        tvText.setText(flacFileName);

        builder.setView(mView);

        builder.setPositiveButton(getResources().getString(R.string.ok),
                new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int id) {

                        ExecutorService scheduledExecutorService =
                                Executors.newScheduledThreadPool(2);

                         ScheduledFuture scheduledFuture1 =
                                (ScheduledFuture) scheduledExecutorService.submit (new Callable() {
                                    public Object call() {
                                        StartEncoder(flacFileName, pathRecordedData);
                                        // if(Debug.ON)Log.e("scheduledExecutorService.submit", "Scheduled task!");
                                        return "Exit1";
                                    }
                                });
                        dialog.cancel();
                    }
                });


        builder.setNegativeButton(getResources().getString(R.string.cancel), (dialog, id) -> dialog.cancel());

        AlertDialog alertDialog = builder.create();
        alertDialog.show();

        Resources res = getResources();
        int bkgrndColour = ResourcesCompat.getColor(res, R.color.colorTealPrimary, null);
        int textColour = ResourcesCompat.getColor(res, R.color.colorTealAccent2, null);

        Button btn = alertDialog.getButton(AlertDialog.BUTTON_POSITIVE);
        btn.setTextColor(textColour);

        ColorDrawable cd = new ColorDrawable(bkgrndColour);

        try {
            Objects.requireNonNull(alertDialog.getWindow()).setBackgroundDrawable(cd);
        }catch (java.lang.NullPointerException e)
        {
            if(Debug.ON)Log.e("Autocord", e.toString());
        }
    }

    private void PlayExportedAudio(EnumReturn et)
    {
        final String[] sFileToDelete = new String[1];
        String[] filenames = new String[et.filesList.length];
        int n = 0;
        for(String s: et.filesList)
        {
            filenames[n++] = s.substring(s.lastIndexOf('/') + 1);
        }
        FilenamesToDateTimes fn2dt = new FilenamesToDateTimes();
        String [] processedFilenames = fn2dt.convert(filenames, et.fileLengths);

        Resources res = getResources();
        int bkgrndColour = ResourcesCompat.getColor(res, R.color.colorTealPrimary, null);
        int textColour = ResourcesCompat.getColor(res, R.color.colorTealAccent2, null);

        AlertDialog.Builder builder = new AlertDialog.Builder(MainActivity.this);
        View mView = LayoutInflater.from(MainActivity.this).inflate(R.layout.dialog_listfiles, null);
        final ListView mListView = mView.findViewById(R.id.list_files); // list_logheader is the ID of the list box in dialog_list
        mListView.setChoiceMode(ListView.CHOICE_MODE_SINGLE);
        mListView.setSelector(android.R.color.darker_gray);

        TextView header = (TextView)getLayoutInflater().inflate(R.layout.list_header, mListView, false);
        header.setText(getResources().getString(R.string.exported_files_hdr1));
        header.setTextColor(textColour);
        mListView.addHeaderView(header);

        header = (TextView)getLayoutInflater().inflate(R.layout.list_header, mListView, false);
        header.setText(getResources().getString(R.string.exported_files_hdr2));
        header.setTextColor(textColour);
        mListView.addHeaderView(header);

        ArrayAdapter<String> adapter = new ArrayAdapter<>(this, R.layout.list_row_light, R.id.listTextViewLight, processedFilenames);
        mListView.setAdapter(adapter);


//*********************************************
        int numberOfItems = adapter.getCount();
        if(numberOfItems > 4)numberOfItems = 4; // show a maximum of 4 items
        numberOfItems += 2; // 2 headers

        // Get height of an item
        View item = adapter.getView(0, null, mListView);
        float px = 500 * (mListView.getResources().getDisplayMetrics().density);
        item.measure(View.MeasureSpec.makeMeasureSpec((int)px, View.MeasureSpec.AT_MOST), View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED));

        int totalItemsHeight = numberOfItems * item.getMeasuredHeight();
        int totalDividersHeight = (numberOfItems - 1) * mListView.getDividerHeight();
        int totalPadding = mListView.getPaddingTop() + mListView.getPaddingBottom();

        // Set list height.
        ViewGroup.LayoutParams params = mListView.getLayoutParams();
        params.height = totalItemsHeight + totalDividersHeight + totalPadding;
        mListView.setLayoutParams(params);
        mListView.requestLayout();
//***********************************************

        ImageButton btnStart = mView.findViewById(R.id.imageBtnPlayerStart);
        ImageButton btnPause = mView.findViewById(R.id.imageBtnPlayerPause);
        ImageButton btnDelete = mView.findViewById(R.id.imageBtnDelete);
        ImageButton btnShare = mView.findViewById(R.id.imageBtnShare);
        TextView tvTimestamp = mView.findViewById(R.id.timestamp);
        TextView tvPeakFreq = mView.findViewById(R.id.peakfreq);
        TextView tvPeakDB = mView.findViewById(R.id.peaklevel);
        SeekBar seekBar = mView.findViewById(R.id.seek);

        if (Debug.ON) Log.v(TAG, "Creating AudioMediaPlayer");
        Context context = getApplicationContext();
        if(bUIModeSimplified)
        {
            player = new AudioMediaPlayer(context, tvTimestamp, null, null, seekBar);
            tvPeakFreq.setVisibility(View.INVISIBLE);
            tvPeakDB.setVisibility(View.INVISIBLE);
        }else
        {
            player = new AudioMediaPlayer(context, tvTimestamp, tvPeakFreq, tvPeakDB, seekBar);
        }

        final boolean[] bFileSelected = {false};
        AdapterView.OnItemClickListener listClick = new AdapterView.OnItemClickListener()
        {
            @Override
            public void onItemClick(AdapterView parent, View view, int position, long rowID)
            {
                // Clicking on first header:  position: 0, rowID: -1
                // Clicking on second header: position: 1, rowID: -1
                // Clicking on first item:    position: 2, rowID:  0
                if(rowID >= 0)
                {
                    // We want the full pathname, not the file name stored in the list
                    String itemValue = et.filesList[(int) rowID];   // (String) parent.getItemAtPosition(i);
                    sFileToDelete[0] = itemValue;
                    if (Debug.ON) Log.v(TAG, "Calling AudioTrackPlayer.prepare with...");
                    if (Debug.ON) Log.v(TAG, itemValue);
                    player.prepare(itemValue);
                    bFileSelected[0] = true;
                    btnStart.setVisibility(View.VISIBLE);
                    btnPause.setVisibility(View.INVISIBLE);
                }
            }
        };

        mListView.setOnItemClickListener(listClick);


        ImageButton.OnClickListener playPauseToggleListener = new ImageButton.OnClickListener()
        {
            @Override
            public void onClick(View v)
            {
                if(!bFileSelected[0])
                {
                    Toast.makeText(MainActivity.this, "You need to select a track to play", Toast.LENGTH_LONG).show();
                }else
                {
                    if (player.getPause())
                    {
                        if (player.play())
                        {
                            // if (Debug.ON) Log.v(TAG, "Playing!");
                            btnPause.setVisibility(View.VISIBLE);
                            btnStart.setVisibility(View.INVISIBLE);
                        } else
                        {
                            Toast.makeText(MainActivity.this, "Sorry - there is some problem with this track", Toast.LENGTH_LONG).show();
                        }
                    } else
                    {
                        btnStart.setVisibility(View.VISIBLE);
                        btnPause.setVisibility(View.INVISIBLE);
                        // if (Debug.ON) Log.v(TAG, "Pausing!");
                        player.pause();
                    }
                }
            }
        };

        ImageButton.OnClickListener btnShareListener = new ImageButton.OnClickListener()
        {
            @Override
            public void onClick(View v)
            {
                if(!bFileSelected[0])
                {
                    Toast.makeText(MainActivity.this, "You need to select a track to share", Toast.LENGTH_LONG).show();
                }else
                {
                    shareAudioFileDialog(sFileToDelete[0]);
                }
            }
        };

        ImageButton.OnClickListener btnDeleteListener = new ImageButton.OnClickListener()
        {
            @Override
            public void onClick(View v)
            {
                if(!bFileSelected[0])
                {
                    Toast.makeText(MainActivity.this, "You need to select a track to delete", Toast.LENGTH_LONG).show();
                }else
                {
                    confirmDeleteDialog(sFileToDelete[0], TrackDeleter.DeleterActions.MARK_FOR_DELETE);
                }
            }
        };

        btnStart.setOnClickListener(playPauseToggleListener);
        btnPause.setOnClickListener(playPauseToggleListener);
        btnShare.setOnClickListener(btnShareListener);
        btnDelete.setOnClickListener(btnDeleteListener);
        //***************************************************


        builder.setView(mView);

        builder.setPositiveButton(getResources().getString(R.string.close),
                new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int id) {
                        player.stop();
                        player = null;
                        dialog.cancel();
                    }
                });

        builder.setOnDismissListener(new DialogInterface.OnDismissListener() {
            @Override
            public void onDismiss(DialogInterface dialogInterface) {
                if(player != null)
                {
                    player.stop();
                    player = null;
                }
                if(trackDeleter.getCount() > 0)
                {
                    confirmDeleteDialog(null, TrackDeleter.DeleterActions.DELETE);
                }
            }
        });

        AlertDialog alertDialog = builder.create();
        alertDialog.show();

        Button btn = alertDialog.getButton(AlertDialog.BUTTON_POSITIVE);
        btn.setTextColor(textColour);

        ColorDrawable cd = new ColorDrawable(bkgrndColour);

        try {
            Objects.requireNonNull(alertDialog.getWindow()).setBackgroundDrawable(cd);
        }catch (java.lang.NullPointerException e)
        {
            if(Debug.ON)Log.e("Autocord", e.toString());
        }
    }

    private void shareAudioFileDialog(String sFileToShare)
    {
        Uri uri = FileProvider.getUriForFile(getApplicationContext(), "com.tropicalcoder.autocord.provider", new File(sFileToShare));
        Intent shareIntent = new Intent(Intent.ACTION_SEND);
        shareIntent.setType("audio/*");
        shareIntent.putExtra(Intent.EXTRA_STREAM, uri);
        shareIntent.setFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
        int n = sFileToShare.lastIndexOf('/');
        String fn = sFileToShare.substring(n + 1, sFileToShare.length());
        String title = "Sharing " + fn;
        startActivity(Intent.createChooser(shareIntent, title));
    }


    private void confirmDeleteDialog(String sFileToDelete, TrackDeleter.DeleterActions action)
    {
        AlertDialog.Builder builder = new AlertDialog.Builder(this);
        View mView = LayoutInflater.from(MainActivity.this).inflate(R.layout.dialog_deletetrack, null);

        final TextView tvText = mView.findViewById(R.id.view_text);

        if(action == TrackDeleter.DeleterActions.MARK_FOR_DELETE)
        {
            tvText.setText(R.string.mark_for_delete);
        }else
        {
            if(trackDeleter.getCount() > 1)
            {
                tvText.setText(R.string.delete_tracks);
            }else
            {
                tvText.setText(R.string.delete_track);
            }
        }

        builder.setView(mView);
        builder.setPositiveButton(this.getResources().getString(R.string.ok),
                new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int id) {
                        if(action == TrackDeleter.DeleterActions.MARK_FOR_DELETE)
                        {
                            if(Debug.ON)Log.e("confirmDeleteDialog", "sFileToDelete: " + sFileToDelete);
                            trackDeleter.addFile(sFileToDelete);
                        }else
                        {
                            while(trackDeleter.getCount() > 0)
                            {
                                trackDeleter.deleteNext();
                            }
                        }
                        dialog.cancel();
                    }
                });


        builder.setNegativeButton(this.getResources().getString(R.string.cancel), // (dialog, id) -> dialog.cancel());
                new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int id) {
                        if(action == TrackDeleter.DeleterActions.DELETE)
                        {
                            if(Debug.ON)Log.e("confirmDeleteDialog", "cancel deletions");
                            trackDeleter.clear();
                        }
                        dialog.cancel();
                    }
                });

        AlertDialog alertDialog = builder.create();
        alertDialog.show();

        Resources res = getResources();
        int bkgrndColour = ResourcesCompat.getColor(res, R.color.colorTealPrimary, null);
        int textColour = ResourcesCompat.getColor(res, R.color.colorTealAccent2, null);

        Button btn = alertDialog.getButton(AlertDialog.BUTTON_POSITIVE);
        btn.setTextColor(textColour);

        ColorDrawable cd = new ColorDrawable(bkgrndColour);

        try {
            Objects.requireNonNull(alertDialog.getWindow()).setBackgroundDrawable(cd);
        }catch (java.lang.NullPointerException e)
        {
            if(Debug.ON)Log.e("Autocord", e.toString());
        }

    }


}

