package com.tropicalcoder.autocord;

import android.util.Log;

import java.io.File;
import java.util.ArrayList;

import static android.content.ContentValues.TAG;

class TrackDeleter
{
    public static enum DeleterActions  {MARK_FOR_DELETE, DELETE};

    ArrayList<Object> files;

    public TrackDeleter()
    {
        files = new ArrayList<>();
    }

    public void addFile(Object pathname)
    {
        if(!files.contains(pathname))
        {
            files.add(pathname);
        }
    }

    public int getCount()
    {
        return files.size();
    }

    public String getNext()
    {
        if(files.size() < 1)
        {
            return null;
        }
        return (String) files.get(files.size() - 1);
    }

    public void deleteNext()
    {
        if(files.size() < 1)
        {
            return;
        }
        int filesIndex = files.size() - 1;
        String pathname = (String) files.get(filesIndex);

        // There can be an issue here if we have two files that share the same log
        // ie: and .m4a and a .flac with the same name
        // In this case, we don't want to delete the log,
        // because it will be needed by the remaining track
        int extIndex = pathname.indexOf('.');
        String sExtension =  pathname.substring(extIndex + 1);
        if(Debug.ON) Log.e("TrackDeleter->deleteNext", "sExtension: " + sExtension);
        String tmp = null;
        if(sExtension.equals("m4a"))
        {
           // Is there a .flac with the same name?
           tmp = pathname.substring(0, extIndex + 1);
           tmp = tmp.concat("flac");
            if(Debug.ON) Log.e("TrackDeleter->deleteNext", "Possible other: " + tmp);
        }

        if(sExtension.equals("flac"))
        {
            // Is there a .m4a with the same name?
            tmp = pathname.substring(0, extIndex + 1);
            tmp = tmp.concat("m4a");
            if(Debug.ON) Log.e("TrackDeleter->deleteNext", "Possible other: " + tmp);
        }

        delete(pathname);

        if(tmp != null)
        {
            File imagePath = new File(tmp);
            if (imagePath.exists())
            {
                if (Debug.ON) Log.e("TrackDeleter->deleteNext", "Found other: " + tmp);
                files.remove(filesIndex);
                return; // Don't delete the .txt
            }
        }

        String textpathname = pathname.substring(0, extIndex + 1);
        textpathname = textpathname.concat("txt");
        if(Debug.ON) Log.e("TrackDeleter->deleteNext", "pathname: " + pathname);
        if(Debug.ON) Log.e("TrackDeleter->deleteNext", "textpathname: " + textpathname);

        // Delete the log file...
        delete(textpathname);

        files.remove(filesIndex);
    }

    public void clear()
    {
        files.clear();
    }

    private void delete(String pathname)
    {
        File imagePath = new File(pathname);
        if (imagePath.exists())
        {
            if (imagePath.delete())
            {
                if(Debug.ON)Log.v(TAG, "Deleted " + imagePath);
            }
        }
    }
}
