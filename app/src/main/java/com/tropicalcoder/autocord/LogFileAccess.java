package com.tropicalcoder.autocord;

import android.content.Context;
import android.util.Log;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.io.IOException;
import java.util.Formatter;

import static android.content.ContentValues.TAG;
import static java.lang.Math.floor;
import static java.lang.Math.log10;

public class LogFileAccess
{
    private File filePath;
    private BufferedReader bufferedReader = null;


    // Assumes LogFileExists() or ReadLogHeader() was called previously
    // Returns file length in bytes
/*
    public int GetFileLength()
    {
        return FileLength;
    }
*/
    public void Close()
    {
        if (bufferedReader != null)
        {
            try
            {
                bufferedReader.close();
            } catch (IOException e)
            {
                e.printStackTrace();
            }
        }
    }

    public boolean LogFileExists(Context context, boolean bDeleteLog)
    {
        File imagePath = new File(context.getExternalFilesDir(null), "data");
        if (!imagePath.exists())
        {
            return true; // Path must exist. It was created with calls to MainActivity.getAudioFilePath()
        }

        File newFile = new File(imagePath, "RecordedData.hdr");
        filePath = newFile.getAbsoluteFile();
        // Log.v(TAG, "Absolute logFile path: " + filePath.getAbsoluteFile());

        int fileLength = 0;
        if (newFile.exists())
        {
            fileLength = (int) newFile.length();
            if(Debug.ON)Log.v(TAG, "RecordedData.hdr length: " + fileLength);
            if (bDeleteLog)
            {
                if (newFile.delete())
                {
                    if(Debug.ON)Log.v(TAG, "Deleted pre-existing RecordedData.hdr");
                }
                return true;
            }
            return false;
        }
        return true;
    }

    public void Rewind()
    {
            if (Debug.ON) Log.v(TAG, "In Rewind");
            Close();
            if(filePath != null)
            {
                if (Debug.ON) Log.v(TAG, filePath.getPath());
                FileReader fileReader = null;
                try
                {
                    fileReader = new FileReader(filePath);
                } catch (IOException e)
                {
                    if (Debug.ON) Log.v(TAG, "IOException20");
                    e.printStackTrace();
                }
                if (fileReader != null)
                {
                    bufferedReader = new BufferedReader(fileReader);
                    if (Debug.ON) Log.v(TAG, "bufferedReader.reset");
                }
            }else
            {
                if (Debug.ON) Log.v(TAG, "filePath is null!");
            }
    }

    String GetDateTime(Context context, AudioEncoder.ENCODER_TYPE encoderType)
    {
        if (LogFileExists(context, false)) return null;

        Rewind();

        String sDateTime = null;
        try
        {
            // in = new BufferedReader(new FileReader(new File(context.getFilesDir(), filePath.getAbsolutePath())));
            // Returns null if the end of the stream has been reached
            FileReader fileReader = new FileReader(filePath);
            bufferedReader = new BufferedReader(fileReader);
            String s = bufferedReader.readLine();
            // Date: Friday, April/03/2020 - Time started: 08:30:40
            // Parse into 2 strings
            if(Debug.ON)Log.v(TAG, "s: " + s);

            // int n = 0;
            if (s != null)
            {
                // Parse out the month
                String month;
                if(s.contains("January"))
                {
                    month = "01";
                }else if(s.contains("February"))
                {
                    month = "02";
                }else if(s.contains("March"))
                {
                    month = "03";
                }else if(s.contains("April"))
                {
                    month = "04";
                }else if(s.contains("May"))
                {
                    month = "05";
                }else if(s.contains("June"))
                {
                    month = "06";
                }else if(s.contains("July"))
                {
                    month = "07";
                }else if(s.contains("August"))
                {
                    month = "08";
                }else if(s.contains("September"))
                {
                    month = "09";
                }else if(s.contains("October"))
                {
                    month = "10";
                }else if(s.contains("November"))
                {
                    month = "11";
                }else if(s.contains("December"))
                {
                    month = "12";
                }else
                {
                    month = "--";
                }
                // Log.v(TAG, "month: " + month);

                // Get day of the month
                String day;
                int indexDelimiterA = s.indexOf('/') + 1;
                int indexDelimiterB = s.lastIndexOf('/');
                day = s.substring(indexDelimiterA, indexDelimiterB);
                // Log.v(TAG, "day: " + day);

                // Get the year
                String year = s.substring(indexDelimiterB + 1, indexDelimiterB + 5);
                // Log.v(TAG, "year: " + year);

                // Get the time
                String time;
                indexDelimiterA = s.lastIndexOf(' ') + 1;
                time = s.substring(indexDelimiterA);
                // Log.v(TAG, "time: " + time);

                // Get the hour
                String hour = time.substring(0, 2);
                //Log.v(TAG, "hour: " + hour);

                String minutes;
                indexDelimiterA = time.indexOf(':') + 1;
                indexDelimiterB = time.lastIndexOf(':');
                minutes = time.substring(indexDelimiterA, indexDelimiterB);
                //Log.v(TAG, "minutes: " + minutes);

                String seconds = time.substring(indexDelimiterB + 1);
                //Log.v(TAG, "seconds: " + seconds);

                StringBuilder sbuf = new StringBuilder();
                Formatter fmt = new Formatter(sbuf);
                if(encoderType == AudioEncoder.ENCODER_TYPE.AAC)
                {
                   fmt.format("%s%s%s%s%s%s.m4a", year, month, day, hour, minutes, seconds);
                }else
                {
                   fmt.format("%s%s%s%s%s%s.flac", year, month, day, hour, minutes, seconds);
                }
                sDateTime = sbuf.toString();
                if(Debug.ON)Log.v(TAG, "Filename: " + sDateTime);

            } else return null;
        } catch (IOException e)
        {
            if(Debug.ON)Log.v(TAG, "IOException: " + e);
        }

        return sDateTime;
    }


    String[] ReadLogHeader(Context context, int currentAudioFileLength, float fPeakAmplitude, int countContiguousEventGroups, int countTotalEvents)
    {
        if (LogFileExists(context, false)) return null;

        //String sLogHeader = null;
        // StringBuilder stringBuilder = new StringBuilder();
        String[] sLines = new String[9];

        try
        {
            // in = new BufferedReader(new FileReader(new File(context.getFilesDir(), filePath.getAbsolutePath())));
            // Returns null if the end of the stream has been reached
            FileReader fileReader = new FileReader(filePath);
            bufferedReader = new BufferedReader(fileReader);
            String s = bufferedReader.readLine();
            // Date: Friday, April/03/2020 - Time started: 08:30:40
            // Parse into 2 strings
            int n = 0;
            if (s != null)
            {
                int indexDelimiter = s.lastIndexOf('-');
                if (indexDelimiter >= 0)
                {
                    sLines[n++] = s.substring(0, indexDelimiter - 1) + '\n';
                    sLines[n++] = s.substring(indexDelimiter + 2) + '\n';
                }
            } else return null;

            // Reading something like...
            // Min. Trigger frequency: 16.000
            // Max Trigger frequency: 16000.0
            // Trigger dB: -47.0
            for (int line = 1; line < 4; line++)
            {
                sLines[n++] = bufferedReader.readLine();
            }
            for (n = 0; n < 5; n++)
            {
                if (sLines[n] == null || sLines[n].equals(""))
                {
                    sLines[n] = "No info";
                }
            }

            // Now add the file duration
            int samples = currentAudioFileLength  / 2;
            double dSeconds = (double) samples / 32000;
            int seconds = (int) floor(dSeconds + 0.5);
            int minutes = seconds / 60;
            int hours = minutes / 60;

            seconds %= 60;
            minutes %= 60;
            hours %= 100; // // What if >= 100 hours? We roll over to start at 0 hours again

            StringBuilder sbuf = new StringBuilder();
            Formatter fmt = new Formatter(sbuf);

            int kb = currentAudioFileLength / 1024;
            if ((currentAudioFileLength % 1024) > 512) ++kb;

            if (kb < 1000)
            {
                fmt.format("Duration: %02d:%02d:%02d,  %d kb", hours, minutes, seconds, kb);
            } else
            {
                float mb = (float) kb / 1024;
                // Show mb to 1 decimal place
                fmt.format("Duration: %02d:%02d:%02d,  %1.1f mb", hours, minutes, seconds, mb);
            }

            sLines[5] = sbuf.toString();

            sbuf.delete(0, sbuf.length());
            float dB = (float) (20.f * log10(fPeakAmplitude));
            fmt.format("Peak amplitude: %1.3f,  dB: %1.1f", fPeakAmplitude, dB);
            sLines[6] = sbuf.toString();

            sbuf.delete(0, sbuf.length());
            fmt.format("Contigous event groups: %d", countContiguousEventGroups);
            sLines[7] = sbuf.toString();

            sbuf.delete(0, sbuf.length());
            fmt.format("Total events: %d", countTotalEvents);
            sLines[8] = sbuf.toString();

            // String line;
            // while ((line = in.readLine()) != null) stringBuilder.append(line);
        } catch (IOException e)
        {
            Log.v(TAG, "IOException: " + e);
        }

        return sLines;
    }
}

