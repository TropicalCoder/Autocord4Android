package com.tropicalcoder.autocord;

import android.media.AudioFormat;
import android.media.MediaCodec;
import android.media.MediaCodecInfo;
import android.media.MediaCodecList;
import android.media.MediaFormat;
import android.media.MediaMuxer;
import android.util.Log;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.BufferOverflowException;
import java.nio.ByteBuffer;
import java.nio.channels.FileChannel;

import static android.media.MediaCodec.BUFFER_FLAG_END_OF_STREAM;
import static android.media.MediaCodecInfo.CodecProfileLevel.AACObjectLC;
import static android.media.MediaFormat.KEY_AAC_PROFILE;
import static java.lang.Thread.sleep;

// For an ACC encoder/decoder, see...
// https://www.electronicdesign.com/technologies/embedded-revolution/article/21799869/understanding-fullhd-voice-communication-on-androidbased-devices
// Some other references
// https://github.com/OnlyInAmerica/HWEncoderExperiments/blob/audiotest/HWEncoderExperiments/src/main/java/net/openwatch/hwencoderexperiments/AudioEncodingTest.java#L134
// https://play.google.com/store/apps/details?id=com.hiqrecorder.full
// https://android.googlesource.com/platform/cts/+/master/tests/tests/media/src/android/media/cts/MediaCodecTest.java
// https://stackoverflow.com/questions/35884600/android-mediacodec-encode-and-decode-in-asynchronous-mode
// https://github.com/mstorsjo/android-decodeencodetest
// https://android.googlesource.com/platform/cts/+/jb-mr2-release/tests/tests/media/src/android/media/cts/EncoderTest.java
// https://bigflake.com/mediacodec/
/*
Encoding test with 1 1/2 hour track of mostly just noise

Encoding speed observed on a Samsung Galaxy Note 8
AAC 64K  957,677.19774 samples per second - 2.00485 minutes per hour of audio
AAC 128K 814,946.46154 samples per second - 2.35598 minutes per hour of audio
FLAC     792,097.49533 samples per second - 2.42394 minutes per hour of audio

Compression ratio
AAC 64K  0.1270 (Estimated as 1/2 that of AAC 128K)
AAC 128K 0.2539
FLAC     0.6184

Samples in 1 minute 1,920,000
*/

class AudioEncoder
{
    public enum ENCODER_TYPE{FLAC, AAC}
    private native boolean prepareEncodeBuffer();
    private final int SAMPLE_RATE = 32000;
    private final int NUM_CHANNELS = 1;
    private final int FRAME_SIZE = 2048;
    private final int nSamplesPerBuffer = FRAME_SIZE;
    private final int nInBufferSize = nSamplesPerBuffer * 2; // 2 bytes each for PCM mono
   // private final int nInBufferSize = nSamplesPerBuffer * 4; // 4 bytes each for float
    private int nTotalSamplesToEncode = 0;
    private int nEncodedDataSize = 0;
    private int currentOffset = 0;
    private int countBuffers = 0;
    private int countTimesIdle = 0;
    private int trackIndex = 0;
    private int bitRate = 0; // 128000 or 64000 for m4a, not relevant for flac
    private File outfile;
    private FileOutputStream fosAudio = null;
    FileChannel channel = null;

    public static ByteBuffer encoderInputBuffer = null;

    private MediaCodec mediaCodec;
    private MediaMuxer mediaMuxer = null;
    private MediaCodec.BufferInfo bufinfo = null;
    private final String outpath;
    private final ENCODER_TYPE desiredCodec;

    AudioEncoder(String pathName, int samplesToEncode, ENCODER_TYPE encoderType, int encoderBitRate)
    {
        desiredCodec = encoderType;
        bitRate = encoderBitRate;
        nTotalSamplesToEncode = samplesToEncode;
        countBuffers = nTotalSamplesToEncode / nSamplesPerBuffer;
        encoderInputBuffer = ByteBuffer.allocateDirect(nInBufferSize);
        if (Debug.ON) Log.v("AudioEncoder", "encoderInputBuffer size bytes: " + nInBufferSize);

        outpath = pathName;

        if(desiredCodec == ENCODER_TYPE.FLAC)
        {
            // Prepare output file
            outfile = new File(pathName);

            try
            {
                fosAudio = new FileOutputStream(outfile);
            } catch (FileNotFoundException e)
            {
                if (Debug.ON) Log.v("AudioEncoder", "create FileOutputStream failed!");
                e.printStackTrace();
                return;
            }

            // PrepareFlacHeader(); // Only for emulator!!!

            channel = fosAudio.getChannel();
        }

        if (Debug.ON) Log.v("AudioEncoder", "nTotalSamplesToEncode: " + nTotalSamplesToEncode);
    }


    public int getInputBytesExpected()
    {
        return nInBufferSize;
    }

/*
To adapt code to a different codec,
go to AudioStorage.FileExtensionFilter and change the extension according to the codec
go to LogFileAccess.GetDateTime(Context context)
scroll to bottom of that function and change the extension in...
                 fmt.format("%s%s%s%s%s%s.m4a", year, month, day, hour, minutes, seconds);
again according to the codec.
*/
    public boolean createCodec()
    {
        if(!codecCreate())
        {
            CloseOutputFile();
            DeleteOutfile();
            return false;
        }
        return true;
    }

    public boolean codecCreate()
    {
        // Check if there was any problem with the file I/O...
        if(nTotalSamplesToEncode == 0)return false;

        MediaCodecList mcl = new MediaCodecList(MediaCodecList.REGULAR_CODECS);

        String name;
        if(desiredCodec == ENCODER_TYPE.FLAC)
        {
           if(channel == null)return false;
           name = mcl.findEncoderForFormat(MediaFormat.createAudioFormat(MediaFormat.MIMETYPE_AUDIO_FLAC, SAMPLE_RATE, NUM_CHANNELS));
        }else
        {
           name = mcl.findEncoderForFormat(MediaFormat.createAudioFormat(MediaFormat.MIMETYPE_AUDIO_AAC, SAMPLE_RATE, NUM_CHANNELS));
        }

        if(name == null)
        {
           return false;
        }

        if (Debug.ON) Log.v("createCodec", "MediaFormat name: " + name);

        try
        {
            mediaCodec = MediaCodec.createByCodecName(name);
        } catch (IOException e)
        {
            if (Debug.ON) Log.v("createCodec", "Failed creating codec");
            e.printStackTrace();
            return false;
        }
        if (Debug.ON) Log.v("createCodec", "Created codec!");

        if(desiredCodec == ENCODER_TYPE.AAC)
        {
            try
            {
                mediaMuxer = new MediaMuxer(outpath, MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4);
            } catch (IOException e)
            {
                if (Debug.ON) Log.v("createCodec", "Failed to create mediaMuxer!");
                e.printStackTrace();
                return false;
            }
            if (Debug.ON) Log.v("createCodec", "Created mediaMuxer!");
        }
        return true;
    }

    public void UnprepareSynchronousOperation()
    {
        if(mediaCodec != null)
        {
            mediaCodec.stop();
            mediaCodec.release();
            mediaCodec = null;
        }
        if(mediaMuxer != null)
        {
            mediaMuxer.stop();
            mediaMuxer.release();
            mediaMuxer = null;
        }

        if (Debug.ON) Log.v("UnprepareSynchronousOperation", "Unprepared!");
    }

    // Return 0 if no buffers available
    // Return 1 if filled a buffer
    // Return -1 for error
    private int CheckInput(long usec)
    {
        if(currentOffset == countBuffers)
        {
            if (Debug.ON) Log.v("CheckInput", "We shouldn't be here - currentOffset == countBuffers");
            return 0;
        }

        int inputBufferId = mediaCodec.dequeueInputBuffer(0);

        if (inputBufferId >= 0)
        {
            ByteBuffer inputBuffer = mediaCodec.getInputBuffer(inputBufferId);
            assert inputBuffer != null;

            // fill inputBuffer with raw audio data
            if(!prepareEncodeBuffer())
            {
                if (Debug.ON) Log.v("CheckInput", "There is no more audio data!");
                return -1;
            }
            encoderInputBuffer.rewind();
            try
            {
                inputBuffer.put(encoderInputBuffer);
            }catch(BufferOverflowException e)
            {
                if (Debug.ON) Log.v("CheckInput", "BufferOverflowException!");
                return -1;
            }
            encoderInputBuffer.rewind();
            ++currentOffset;
            if (currentOffset == countBuffers)
            {
                if (Debug.ON) Log.v("CheckInput", "currentOffset == countBuffers");
                mediaCodec.queueInputBuffer(inputBufferId, 0, nInBufferSize, usec, BUFFER_FLAG_END_OF_STREAM);
            }else
            {
                mediaCodec.queueInputBuffer(inputBufferId, 0, nInBufferSize, usec, 0);
            }
            return 1;
        }
        return 0;
    }

    // Return 0 if no buffers available
    // Return 1 if there was decode output or EoS
    // Return -1 for error
    private int CheckOutput()
    {
        int outputBufferId = mediaCodec.dequeueOutputBuffer(bufinfo, 0);

        if(outputBufferId < 0)
        {
            if(desiredCodec == ENCODER_TYPE.AAC)
            {
                if (outputBufferId == MediaCodec.INFO_OUTPUT_FORMAT_CHANGED)
                {
                    if (Debug.ON) Log.v("CheckOutput", "INFO_OUTPUT_FORMAT_CHANGED!");
                    MediaFormat newFormat = mediaCodec.getOutputFormat();
                    try
                    {
                        trackIndex = mediaMuxer.addTrack(newFormat);
                    }catch(IllegalArgumentException | IllegalStateException e)
                    {
                        e.printStackTrace();
                        return -1;
                    }

                    try
                    {
                        mediaMuxer.start();
                    }catch(IllegalStateException e)
                    {
                        e.printStackTrace();
                        return -1;
                    }
                    return 1;
                }
            }

            if ((bufinfo.flags & BUFFER_FLAG_END_OF_STREAM) > 0)
            {
                if (Debug.ON) Log.v("CheckOutput1", "BUFFER_FLAG_END_OF_STREAM!");
                return 1;
            }

            return 0;
        }

        while (outputBufferId >= 0)
        {
            // The position and limit of the returned buffer are set to the valid output data
            ByteBuffer outputBuffer = mediaCodec.getOutputBuffer(outputBufferId);
            assert outputBuffer != null;
            // outputBuffer.rewind(); // ...so this shouldn't be necessary
            nEncodedDataSize += bufinfo.size;

            if (Debug.ON)
                Log.v("CheckOutput", "getOutputBuffer bufinfo.size: " + bufinfo.size);

            // info.offset // Start of data in the buffer
            // info.size // May be 0 when END_OF_STREAM
            if (bufinfo.size == 0)
            {
                if (Debug.ON) Log.v("CheckOutput", "getOutputBuffer bufinfo.size == 0");
            } else
            {
                if(desiredCodec == ENCODER_TYPE.FLAC)
                {
                    try
                    {
                        channel.write(outputBuffer);
                    } catch (IOException e)
                    {
                        e.printStackTrace();
                        return -1;
                    }
                }else
                {
                    try
                    {
                        mediaMuxer.writeSampleData(trackIndex, outputBuffer, bufinfo);
                    }catch(IllegalArgumentException | IllegalStateException e)
                    {
                        e.printStackTrace();
                        return -1;
                    }
                }
            }
            mediaCodec.releaseOutputBuffer(outputBufferId, false);

            if ((bufinfo.flags & BUFFER_FLAG_END_OF_STREAM) > 0)
            {
                if (Debug.ON) Log.v("CheckOutput2", "BUFFER_FLAG_END_OF_STREAM!");
                return 1;
            }

            outputBufferId = mediaCodec.dequeueOutputBuffer(bufinfo, 0);
        } // End while (outputBufferId >= 0)
        return 1;
    }

    public void CloseOutputFile()
    {
        // This is the last buffer
        if (Debug.ON) Log.v("CloseOutputFile", "This is the last buffer - currentOffset: " + currentOffset);
        if (Debug.ON) Log.v("CloseOutputFile", "nEncodedDataSize: " + nEncodedDataSize);
        if (Debug.ON) Log.v("CloseOutputFile", "Compression ratio: " + (float)nEncodedDataSize / (float) (nTotalSamplesToEncode * 2));
        if (Debug.ON) Log.v("CloseOutputFile", "countTimesIdle: " + countTimesIdle);

        if(desiredCodec == ENCODER_TYPE.FLAC)
        {
            int filesize = 0;
            try
            {
                filesize = (int) fosAudio.getChannel().position();
            } catch (IOException e)
            {
                e.printStackTrace();
            }
            if (Debug.ON) Log.v("CloseOutputFile", "filesize: " + filesize);
            // if (Debug.ON) Log.v("EncodeBuffer", "biggestOutBuffer size: " + biggestOutBuffer);
            try
            {
                channel.close();
            } catch (IOException e)
            {
                if (Debug.ON) Log.v("CloseOutputFile", "channel.close() exception");
                e.printStackTrace();
            }

            try
            {
                fosAudio.flush();
            } catch (IOException e)
            {
                if (Debug.ON) Log.v("CloseOutputFile", "fosAudio.flush exception");
                e.printStackTrace();
            }
            try
            {
                fosAudio.close();
            } catch (IOException e)
            {
                if (Debug.ON) Log.v("CloseOutputFile", "fosAudio.close exception");
                e.printStackTrace();
            }
        }
    }

    // https://www.hiqrecorder.com/
    // RESOLVING M4A ENCODING PROBLEM
    // https://www.hiqrecorder.com/resolving-m4a-encoding-problem/?utm_source=rss&utm_medium=rss&utm_campaign=resolving-m4a-encoding-problem
    // https://github.com/OnlyInAmerica/HWEncoderExperiments/blob/audiotest/HWEncoderExperiments/src/main/java/net/openwatch/hwencoderexperiments/AudioEncodingTest.java#L134
    public void RunEncoder()
    {
        int flags = 0;
        //long usec = 1000000000L * FRAME_SIZE/SAMPLE_RATE; Turns out, this was wrong. When playing back, Media player reports duration 1000 x longer
        long usec = 1000000L * FRAME_SIZE/SAMPLE_RATE;
        if (Debug.ON) Log.v("RunEncoder", "starting");

        MediaFormat format = new MediaFormat();
        if(desiredCodec == ENCODER_TYPE.FLAC)
        {
           format.setString(MediaFormat.KEY_MIME, "audio/flac");
           format.setInteger(MediaFormat.KEY_FLAC_COMPRESSION_LEVEL, 5);
        }else
        {
           format.setString(MediaFormat.KEY_MIME, "audio/mp4a-latm");
           format.setInteger(KEY_AAC_PROFILE, AACObjectLC); // See https://www.hiqrecorder.com/resolving-m4a-encoding-problem/?utm_source=rss&utm_medium=rss&utm_campaign=resolving-m4a-encoding-problem
           format.setInteger(MediaFormat.KEY_BIT_RATE, bitRate);
        }

        format.setInteger(MediaFormat.KEY_SAMPLE_RATE, SAMPLE_RATE);
        format.setInteger(MediaFormat.KEY_CHANNEL_COUNT, NUM_CHANNELS);
        format.setInteger(MediaFormat.KEY_PCM_ENCODING, AudioFormat.ENCODING_PCM_16BIT);
        // *New - for Samsung Galaxy s20 Ultra
        format.setInteger(MediaFormat.KEY_MAX_INPUT_SIZE, FRAME_SIZE * NUM_CHANNELS * 2); // API 11 only!

        mediaCodec.configure(format, null, null, MediaCodec.CONFIGURE_FLAG_ENCODE);
        if (Debug.ON) Log.v("RunEncoder", "configured codec");
        // if (Debug.ON)CheckFormatSet();
        bufinfo = new MediaCodec.BufferInfo();
        bufinfo.set(0, FRAME_SIZE * NUM_CHANNELS * 2, usec, 0);
        int retvalIn, retvalOut;
        boolean bError = false;
        mediaCodec.start();
        while(bufinfo.flags != BUFFER_FLAG_END_OF_STREAM)
        {
            retvalIn = CheckInput(usec * (currentOffset + 1));
            bufinfo.flags = flags;
            retvalOut = CheckOutput();

            if((retvalIn == 0) &&  (retvalOut == 0))
            {
                // I'm not comfortable with this spinning in a tight loop
                // In one run of a 3 minute file we entered here just over 100 times
                // but only hit this twice in a row a couple of times.
                // I might guess that means that without this little sleep,
                // we could spin thousands or even millions of times
                // using up CPU for nothing.
                // Added ++countTimesIdle and ran it again and this time only got 46
                // Changing FRAME_SIZE from 1024 to 2048 and countTimesIdle dropped to 20
                // These figures were from running on the emulator.
                // if (Debug.ON) Log.v("RunEncoder", "No buffers!");
                if (Debug.ON)++countTimesIdle;
                try
                {
                    sleep(1);
                } catch (InterruptedException e)
                {
                    e.printStackTrace();
                }
            }else if((retvalIn == -1) || (retvalOut == -1))
            {
                bError = true;
                break;
            }
        } // End while(bufinfo.flags != BUFFER_FLAG_END_OF_STREAM)

        CloseOutputFile();
        UnprepareSynchronousOperation();
        if(bError)
        {
            if(Debug.ON)Log.v("RunEncoder", "Error!");
            DeleteOutfile();
            MainActivity.dataReady(-1);
        }
    }

    private void DeleteOutfile()
    {
        if(desiredCodec == ENCODER_TYPE.FLAC)
        {
            if (outfile.delete())
            {
                if(Debug.ON)Log.v("HandleCodecError", "Deleted outfile!");
            }
        }
    }

    private void CheckFormatSet()
    {
        Log.v("formatSet", "Checking format set...");
        MediaFormat formatSet = mediaCodec.getInputFormat();
        if(formatSet.containsKey(MediaFormat.KEY_MAX_INPUT_SIZE))
        {
            Log.v("formatSet", "KEY_MAX_INPUT_SIZE " + formatSet.getInteger(MediaFormat.KEY_MAX_INPUT_SIZE));
        }
        if (formatSet.containsKey(MediaFormat.KEY_SAMPLE_RATE))
        {
            Log.v("formatSet", "KEY_SAMPLE_RATE " + formatSet.getInteger(MediaFormat.KEY_SAMPLE_RATE));
        }
        if (formatSet.containsKey(MediaFormat.KEY_CHANNEL_COUNT))
        {
            Log.v("formatSet", "KEY_CHANNEL_COUNT " + formatSet.getInteger(MediaFormat.KEY_CHANNEL_COUNT));
        }
        if (formatSet.containsKey(MediaFormat.KEY_BIT_RATE))
        {
            Log.v("formatSet", "KEY_BIT_RATE " + formatSet.getInteger(MediaFormat.KEY_BIT_RATE));
        }
        if (formatSet.containsKey(MediaFormat.KEY_PCM_ENCODING))
        {
            int encoding = formatSet.getInteger(MediaFormat.KEY_PCM_ENCODING);
            if(encoding == AudioFormat.ENCODING_PCM_FLOAT)
            {
                Log.v("formatSet", "KEY_PCM_ENCODING = ENCODING_PCM_FLOAT: " + encoding);
            }else if(encoding == AudioFormat.ENCODING_PCM_16BIT)
            {
                Log.v("formatSet", "KEY_PCM_ENCODING = ENCODING_PCM_16BIT " + encoding);
            }
        }
        if (formatSet.containsKey(MediaFormat.KEY_FLAC_COMPRESSION_LEVEL))
        {
            Log.v("formatSet", "KEY_FLAC_COMPRESSION_LEVEL " + formatSet.getInteger(MediaFormat.KEY_FLAC_COMPRESSION_LEVEL));
        }

    }


    private void PrepareFlacHeader()
    {
        int sampleLength = nTotalSamplesToEncode;
        byte[] b = new byte[86];
        int n = 0;
        b[n++] = (byte) 0x66;
        b[n++] = (byte) 0x4C;
        b[n++] = (byte) 0x61;
        b[n++] = (byte) 0x43;
        b[n++] = (byte) 0x00;
        b[n++] = (byte) 0x00;
        b[n++] = (byte) 0x00;
        b[n++] = (byte) 0x22;
        b[n++] = (byte) 0x10;
        b[n++] = (byte) 0x00;
        b[n++] = (byte) 0x10;
        b[n++] = (byte) 0x00;
        b[n++] = (byte) 0x00;
        b[n++] = (byte) 0x04;
        b[n++] = (byte) 0xE8;
        b[n++] = (byte) 0x00;

        b[n++] = (byte) 0x0D;
        b[n++] = (byte) 0x11;
        b[n++] = (byte) 0x07;
        b[n++] = (byte) 0xD0;
        b[n++] = (byte) 0x00;
        b[n++] = (byte) 0xF0;

        // Insert length here at b[22]
        b[n++] = (byte) ((sampleLength >> 24) & 0xFF);
        b[n++] = (byte) ((sampleLength >> 16) & 0xFF);
        b[n++] = (byte) ((sampleLength >> 8) & 0xFF);
        b[n++] = (byte) (sampleLength & 0xFF);
/*
        // Example example 960,000 samples = 00 0E A6 00
        b[n++] = (byte) 0x00; // byte3
        b[n++] = (byte) 0x0E; // byte2
        b[n++] = (byte) 0xA6; // byte1
        b[n++] = (byte) 0x00; // byte0
*/
        b[n++] = (byte) 0xF7;
        b[n++] = (byte) 0xC8;
        b[n++] = (byte) 0x7C;
        b[n++] = (byte) 0x81;
        b[n++] = (byte) 0xAC;
        b[n++] = (byte) 0x95;

        b[n++] = (byte) 0x9E;
        b[n++] = (byte) 0x70;
        b[n++] = (byte) 0xA7;
        b[n++] = (byte) 0x17;
        b[n++] = (byte) 0x22;
        b[n++] = (byte) 0xEE;
        b[n++] = (byte) 0x5F;
        b[n++] = (byte) 0x56;
        b[n++] = (byte) 0x1D;
        b[n++] = (byte) 0xE3;
        b[n++] = (byte) 0x84;
        b[n++] = (byte) 0x00;
        b[n++] = (byte) 0x00;
        b[n++] = (byte) 0x28;
        b[n++] = (byte) 0x20;
        b[n++] = (byte) 0x00;

        b[n++] = (byte) 0x00;
        b[n++] = (byte) 0x00;
        b[n++] = (byte) 0x72;
        b[n++] = (byte) 0x65;
        b[n++] = (byte) 0x66;
        b[n++] = (byte) 0x65;
        b[n++] = (byte) 0x72;
        b[n++] = (byte) 0x65;
        b[n++] = (byte) 0x6E;
        b[n++] = (byte) 0x63;
        b[n++] = (byte) 0x65;
        b[n++] = (byte) 0x20;
        b[n++] = (byte) 0x6C;
        b[n++] = (byte) 0x69;
        b[n++] = (byte) 0x62;
        b[n++] = (byte) 0x46;

        b[n++] = (byte) 0x4C;
        b[n++] = (byte) 0x41;
        b[n++] = (byte) 0x43;
        b[n++] = (byte) 0x20;
        b[n++] = (byte) 0x31;
        b[n++] = (byte) 0x2E;
        b[n++] = (byte) 0x33;
        b[n++] = (byte) 0x2E;
        b[n++] = (byte) 0x31;
        b[n++] = (byte) 0x20;
        b[n++] = (byte) 0x32;
        b[n++] = (byte) 0x30;
        b[n++] = (byte) 0x31;
        b[n++] = (byte) 0x34;
        b[n++] = (byte) 0x31;
        b[n++] = (byte) 0x31;

        b[n++] = (byte) 0x32;
        b[n++] = (byte) 0x35;
        b[n++] = (byte) 0x00;
        b[n++] = (byte) 0x00;
        b[n++] = (byte) 0x00;
        b[n++] = (byte) 0x00;

        if (Debug.ON) Log.v("PrepareFlacHeader", "Num bytes in header: " + n);

        try
        {
            fosAudio.write(b, 0, n);
        } catch (IOException e)
        {
            e.printStackTrace();
        }
    }

    private void GetSupportedCodecs()
    {
        if (Debug.ON)
        {
            MediaCodecList mcl = new MediaCodecList(MediaCodecList.REGULAR_CODECS);
            MediaCodecInfo[] mci = mcl.getCodecInfos();
            int numCodecs = mci.length;
            MediaCodecInfo codecInfo = null;
            for (MediaCodecInfo mediaCodecInfo : mci)
            {
                if (mediaCodecInfo.isEncoder())
                {
                    String[] types = mediaCodecInfo.getSupportedTypes();
                    for (String s : types)
                    {
                        Log.d("Supported codecs", "mime: " + s + " encoder: " + mediaCodecInfo.getName());
                    }
                }
            }
        }
    }
/*
Emulator encoders...
    mime: audio/mp4a-latm encoder: OMX.google.aac.encoder
    mime: audio/3gpp encoder: OMX.google.amrnb.encoder
    mime: audio/amr-wb encoder: OMX.google.amrwb.encoder
    mime: audio/flac encoder: OMX.google.flac.encoder
    mime: video/avc encoder: OMX.google.h264.encoder
    mime: video/3gpp encoder: OMX.google.h263.encoder
    mime: video/mp4v-es encoder: OMX.google.mpeg4.encoder
    mime: video/x-vnd.on2.vp8 encoder: OMX.google.vp8.encoder

From my Note 8

W/AudioCapabilities: Unsupported mime audio/ac4
W/AudioCapabilities: Unsupported mime audio/x-ima
W/AudioCapabilities: Unsupported mime audio/x-ape
W/AudioCapabilities: Unsupported mime audio/eac3-joc
W/AudioCapabilities: Unsupported mime audio/evrc
W/AudioCapabilities: Unsupported mime audio/mpeg-L1
W/AudioCapabilities: Unsupported mime audio/mpeg-L2
W/AudioCapabilities: Unsupported mime audio/qcelp
W/AudioCapabilities: Unsupported mime audio/x-ms-wma
W/AudioCapabilities: Unsupported mime audio/evrc
W/AudioCapabilities: Unsupported mime audio/qcelp

mime: audio/mp4a-latm encoder: OMX.google.aac.encoder
mime: audio/mp4a-latm encoder: OMX.SEC.naac.enc
mime: audio/3gpp encoder: OMX.google.amrnb.encoder
mime: audio/amr-wb encoder: OMX.google.amrwb.encoder
mime: audio/evrc encoder: OMX.SEC.evrc.enc
mime: audio/flac encoder: OMX.google.flac.encoder
mime: audio/qcelp encoder: OMX.SEC.qcelp.enc
*/

}
