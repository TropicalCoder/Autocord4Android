package com.tropicalcoder.autocord;

import java.util.Formatter;

import static java.lang.Math.floor;

class FilenamesToDateTimes
{

    private String getMonth(String digits)
    {
        // Parse out the month
        String month;
        if (digits.contains("01"))
        {
            month = "Jan.";
        } else if (digits.contains("02"))
        {
            month = "Feb.";
        } else if (digits.contains("03"))
        {
            month = "Mar.";
        } else if (digits.contains("04"))
        {
            month = "Apr.";
        } else if (digits.contains("05"))
        {
            month = "May";
        } else if (digits.contains("06"))
        {
            month = "Jun.";
        } else if (digits.contains("07"))
        {
            month = "Jul.";
        } else if (digits.contains("08"))
        {
            month = "Aug.";
        } else if (digits.contains("09"))
        {
            month = "Sep.";
        } else if (digits.contains("10"))
        {
            month = "Oct.";
        } else if (digits.contains("11"))
        {
            month = "Nov.";
        } else if (digits.contains("12"))
        {
            month = "Dec.";
        } else
        {
            month = "???";
        }
        return month;
    }

    // When we worked with .wav files, we used the filelength (minus Riff header len)
    // to determine duration. Now that we use flac files and duration is passed in 'fileLength'
    // This value was derived from "totalSamples * 2" of the encoded audio
    private String process(String Filename, int fileLength)
    {
        String processed = "";

        // Log.v(TAG, Filename);

        // Filename is in the form yyyymmddhhmmss.wav
        // Convert to something like August 21/2020, 11:45:33

        processed = processed.concat(getMonth(Filename.substring(4, 6)));
        processed = processed.concat(" ");
        processed = processed.concat(Filename.substring(6, 8));
        processed = processed.concat("/");
        processed = processed.concat(Filename.substring(2, 4));
        processed = processed.concat(", ");
        processed = processed.concat(Filename.substring(8, 10));
        processed = processed.concat(":");
        processed = processed.concat(Filename.substring(10, 12));
        processed = processed.concat(":");
        processed = processed.concat(Filename.substring(12, 14));
        // Log.v(TAG, processed);
        StringBuilder sbuf = new StringBuilder();
        Formatter fmt = new Formatter(sbuf);

        // Now tack the duration on the end in similar format
        if(fileLength == 0)
        {
            // No log found for this!
            fmt.format("%s    ??:??:??", processed);
        }else
        {
            int samples = fileLength / 2;
            double dSeconds = (double) samples / 32000;
            int seconds = (int) floor(dSeconds + 0.5);
            int minutes = seconds / 60;
            int hours = minutes / 60;

            seconds %= 60;
            minutes %= 60;
            hours %= 100; // // What if >= 100 hours? We roll over to start at 0 hours again

            fmt.format("%s    %02d:%02d:%02d", processed, hours, minutes, seconds);
        }
        return sbuf.toString();
    }

    public String[] convert(String[] Filenames, int[] FileLengths)
    {
        String[] converted = new String[Filenames.length];

        int n = 0;
        for (String filename : Filenames)
        {
            converted[n] = process(filename, FileLengths[n]);
            ++n;
        }

        return converted;
    }
}
