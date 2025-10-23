package com.tropicalcoder.autocord;

import android.content.Context;
import android.util.Log;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileReader;
import java.io.FilenameFilter;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.Objects;

import static android.content.ContentValues.TAG;

class FileExtensionFilterFLAC implements FilenameFilter
{
    public boolean accept(File dir, String name)
    {
        return (name.endsWith(".flac")); // add more conditions here
    }
}

class FileExtensionFilterAAC implements FilenameFilter
{
    public boolean accept(File dir, String name)
    {
        return (name.endsWith(".m4a")); // add more conditions here
    }
}

class EnumReturn
{
    final public String[] filesList;
    final public int[] fileLengths;

    EnumReturn(String[] flist, int[] fLengths)
    {
        filesList = flist;
        fileLengths = fLengths;
    }
}

class AudioStorage
{
    static String MEDIA_PATH;

    AudioStorage(Context context)
    {
      File imagePath = new File(context.getExternalFilesDir(null), "Recordings");
      if(!imagePath.exists())
      {
        boolean bDirectoryCreationStatus = imagePath.mkdirs();
        if (Debug.ON)
            Log.v("getAudioRecordingFilePath", "directoryCreationStatus: " + bDirectoryCreationStatus);
      }
      MEDIA_PATH = imagePath.getAbsolutePath();
      if (Debug.ON)
            Log.v("AudioStorage constructor", "MEDIA_PATH: " + MEDIA_PATH);
    }



    static String GetExternalMediaPath(String sFilename)
    {
        File imagePath = new File(MEDIA_PATH);

        if(!imagePath.exists())
        {
            boolean bDirectoryCreationStatus = imagePath.mkdirs();
            if(Debug.ON)Log.v("GetExternalMediaPath", "directoryCreationStatus: " + bDirectoryCreationStatus);
        }

        String pathname = MEDIA_PATH + File.separator + sFilename;
        if(Debug.ON)Log.v("GetExternalMediaPath: pathname", pathname);
        return pathname;
    }

    static boolean haveExportedAudio()
    {
        int countItems = 0;

        File home = new File(MEDIA_PATH);
        if (home.listFiles(new FileExtensionFilterAAC()) != null)
        {
            if (Objects.requireNonNull(home.listFiles(new FileExtensionFilterAAC())).length > 0)
            {
                for (File file : Objects.requireNonNull(home.listFiles(new FileExtensionFilterAAC())))
                {
                    ++countItems;
                }
            }
        }
        if (home.listFiles(new FileExtensionFilterFLAC()) != null)
        {
            if (Objects.requireNonNull(home.listFiles(new FileExtensionFilterFLAC())).length > 0)
            {
                for (File file : Objects.requireNonNull(home.listFiles(new FileExtensionFilterFLAC())))
                {
                    ++countItems;
                }
            }
        }

        if (Debug.ON) Log.v("haveExportedAudio", "countItems: " + countItems);
        return (countItems > 0);
    }

    private static int parseLog(String pathName)
    {
        int indexExtension = pathName.lastIndexOf(".");
        String fileName = pathName.substring(0, indexExtension);
        fileName = fileName.concat(".txt");
        if (Debug.ON) Log.v(TAG, "Log file name: " + fileName);

        FileReader fileReader;
        try
        {
            fileReader = new FileReader(fileName);
        } catch (FileNotFoundException e)
        {
            e.printStackTrace();
            return 0;
        }
        BufferedReader bufferedReader = new BufferedReader(fileReader);


        String s = "-";

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
        int totalSamples = 0;
        if (s != null)
        {
            String samplesRecorded = s.substring(19);
            if (Debug.ON) Log.v(TAG, samplesRecorded);
            totalSamples = Integer.parseInt(samplesRecorded);
            if (Debug.ON) Log.v(TAG, "totalSamples: " + totalSamples);
        }
        return totalSamples * 2; // Returned as bytes
    }

    static EnumReturn EnumerateExportedAudio()
    {
        ArrayList<String> filesList = new ArrayList<>();
        ArrayList<Integer> fileLengths = new ArrayList<>();

        if (Debug.ON) Log.v("EnumerateExportedAudio", MEDIA_PATH);
        int countItems = 0;

        File home = new File(MEDIA_PATH);
        if (home.listFiles(new FileExtensionFilterAAC()) != null)
        {
            if (Objects.requireNonNull(home.listFiles(new FileExtensionFilterAAC())).length > 0)
            {
                for (File file : Objects.requireNonNull(home.listFiles(new FileExtensionFilterAAC())))
                {
                    // Have to open the log file and get num samples
                    // If no log found, length will be zero
                    int audioBytes = parseLog(file.getPath());
                    filesList.add(file.getPath());
                    fileLengths.add(audioBytes);
                    ++countItems;
                    if (Debug.ON)Log.v("EnumerateExportedAudio", "fn: " + file.getPath() + ", len: " + audioBytes);
                }
            }
        }
        if (home.listFiles(new FileExtensionFilterFLAC()) != null)
        {
            if (Objects.requireNonNull(home.listFiles(new FileExtensionFilterFLAC())).length > 0)
            {
                for (File file : Objects.requireNonNull(home.listFiles(new FileExtensionFilterFLAC())))
                {
                    // Have to open the log file and get num samples
                    // If no log found, length will be zero
                    int audioBytes = parseLog(file.getPath());
                    filesList.add(file.getPath());
                    fileLengths.add(audioBytes);
                    ++countItems;
                    if (Debug.ON)Log.v("EnumerateExportedAudio", "fn: " + file.getPath() + ", len: " + audioBytes);
                }
            }
        }

        if(countItems > 0)
        {
            HashMap<String, Integer> map = new HashMap<>();

            for(int i = 0; i < countItems; i++)
            {
                map.put(filesList.get(i), fileLengths.get(i));
            }

            Collections.sort(filesList, Collections.reverseOrder());

            String[] retList = new String[countItems];
            int[] retLens = new int[countItems];
            for(int i = 0; i < countItems; i++)
            {
                retList[i] = filesList.get(i);
                retLens[i] = map.get(retList[i]);
                if (Debug.ON) Log.v("After sort", "fn: " + retList[i] + ", len: " + retLens[i]);
            }
            return new EnumReturn(retList, retLens);
        }

        return null;
    }

}
