package com.tropicalcoder.autocord;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Rect;
import android.util.AttributeSet;
import android.util.Log;
import android.view.View;

import java.util.Formatter;

import static com.tropicalcoder.autocord.MainActivity.FRAMEDGRAPH_HEIGHT;
import static com.tropicalcoder.autocord.MainActivity.FRAMEDGRAPH_WIDTH;



class CanvasView extends View
{
    private final int MINMAX_SEPARATION = 16;
    // private static int count = 0;
    private native float renderGraph(Bitmap bitmap);
    private native void clearBitmap(Bitmap bitmap);
    private native float getFrequencyAtGraphCoord(int graphX);
    private native int getGraphCoordAtFrequency(float f);
    private native void setTriggerDB(float dB);
    private native void setTriggerFreqs(float minFreq, float maxFreq);
    // private Context savedContext; // tmp dbg
    // Trigger Params
    // public enum TRIGGER_PARAMS { SETDB, SETMINFREQ, SETMAXFREQ, LOCK}
    // public static TRIGGER_PARAMS curTriggerParam = TRIGGER_PARAMS.LOCK;

    private Bitmap mMainBitmap;
    private final Bitmap mLegend;
    private Rect rGraph;
    private Rect rGraphFrame;
    private Rect rLegendSrc;
    private Rect rLegendDst;
    private final Paint paintLine;
    private final Paint paintLineDim;
    private float dBscale;
    private float fPeakDB;
    public float dBTrigger;
    public float minDBTrigger;
    public float minFreqTrigger;
    public float maxFreqTrigger;
    private int nTriggerDB_y;
    private int nFreqTriggerMinX;
    // private int nFreqTriggerMaxX;
    private int nInitialTriggerMinFreq_x;
    private int nInitialTriggerMaxFreq_x;
    private int nTriggerMinFreq_x;
    private int nTriggerMaxFreq_x;
    // private boolean bClearRecordingLamp;
    private boolean bShowDBTriggerLine;
    private boolean bMinFreqTriggerLine;
    private boolean bMaxFreqTriggerLine;
    private boolean bAccumulatingPeaks;
    private boolean bScrnTooSmall;
    private boolean bInitializedTriggerParams = false;

    public CanvasView(Context context, AttributeSet attrs)
    {
        super(context, attrs);

        BitmapFactory.Options ops = new BitmapFactory.Options();
        ops.inMutable = true;
        Resources res = getResources();
        mLegend = BitmapFactory.decodeResource(res, R.drawable.legend, ops);
        paintLine = new Paint();
        paintLine.setColor(Color.WHITE);
        paintLine.setStyle(Paint.Style.STROKE);
        paintLine.setStrokeWidth(2.0f);

        paintLineDim = new Paint();
        paintLineDim.setColor(Color.DKGRAY);
        paintLineDim.setStyle(Paint.Style.STROKE);
        paintLineDim.setStrokeWidth(2.0f);

        Paint paintText = new Paint();
        // paint.setColor(getResources().getColor(android.R.color.holo_green_light));
        paintText.setColor(Color.MAGENTA);
        paintText.setStyle(Paint.Style.FILL);
        paintText.setTextSize(64);
        paintText.setStrokeWidth(1.0f);

        minDBTrigger = -96.33f;
        minFreqTrigger = 16.f;
        maxFreqTrigger = 16000.f;

        // bClearRecordingLamp = false;
    }

    @Override
    protected void onSizeChanged(int width, int height, int oldw, int oldh)
    {
        // Found I was getting random call to onSizeChanged()
        // after hours of running app. This will reset
        // trigger params. Need to prevent that...
        if(bInitializedTriggerParams)return;

        if((width < FRAMEDGRAPH_WIDTH) || (height < FRAMEDGRAPH_HEIGHT * 2))
        {
            bScrnTooSmall = true;
            return;
        }

        mMainBitmap = Bitmap.createBitmap(width, height, Bitmap.Config.RGB_565);

        // int FRAMEDGRAPH_WIDTH = 482 * 2;
        // int FRAMEDGRAPH_HEIGHT = 322;

        int imageWidth = mMainBitmap.getWidth();
        int imageHeight = mMainBitmap.getHeight();

        if(Debug.ON)Log.v("onSizeChanged", "imageWidth: " + imageWidth + ", imageHeight: " + imageHeight);
        // int yOffset = 48;
        // float fHeight = (imageHeight - yOffset);

        // Scale the height of the graph according to the height of the bitmap
        float scaleHeight, fHeight;

        int yOffset = 48;
        if(imageWidth < imageHeight)
        {
            // Phone is vertical, take half the height
            scaleHeight = (float)(imageHeight / 2) / FRAMEDGRAPH_HEIGHT;
            fHeight = (float) FRAMEDGRAPH_HEIGHT * scaleHeight;
        }else
        {
            // Phone is horizontal, take 3/4 the height
            scaleHeight = ((float)imageHeight * 3 / 4.f) / FRAMEDGRAPH_HEIGHT;
            fHeight = (float) FRAMEDGRAPH_HEIGHT * scaleHeight;
            yOffset /= 2;
        }

        rGraph = new Rect();
        rGraph.left = imageWidth / 2 - FRAMEDGRAPH_WIDTH / 2;
        rGraph.top = yOffset;
        rGraph.right = rGraph.left + FRAMEDGRAPH_WIDTH;
        rGraph.bottom = rGraph.top + (int)fHeight;

        // Default sensitivity = ~47 dB
        // maxdB = 20.0 * log10(1.0/65536.0)
        // Log.v("onSizeChanged", "(fHeight - 2): " + (fHeight - 2));
        dBscale = (fHeight - 2) / 96.33f; // height / maxdB
        // Log.v("onSizeChanged", "dBscale: " + dBscale);
        nTriggerDB_y = rGraph.top + (int)(dBscale * 96.33f);
        // Log.v("onSizeChanged", "nTriggerDB_y: " + nTriggerDB_y);
        dBTrigger = -(float)(nTriggerDB_y - rGraph.top) / dBscale;
        // Log.v("onSizeChanged", "Default dBTrigger: " + dBTrigger);

        nTriggerMinFreq_x = nInitialTriggerMinFreq_x = nFreqTriggerMinX = rGraph.left + 3; // (These adjustments to avoid overwriting frame)
        // nTriggerMaxFreq_x = nInitialTriggerMaxFreq_x = nFreqTriggerMaxX = rGraph.right - 2;
        nTriggerMaxFreq_x = nInitialTriggerMaxFreq_x = rGraph.right - 2;

        rGraphFrame = new Rect();
        rGraphFrame.left = rGraph.left;
        rGraphFrame.top = rGraph.top;
        rGraphFrame.right = rGraph.right;
        rGraphFrame.bottom = rGraph.bottom + mLegend.getHeight();

        if(Debug.ON)Log.v("onSizeChanged", "rGraph.left: " + rGraph.left + ", rGraph.top: " + rGraph.top);
        if(Debug.ON)Log.v("onSizeChanged", "rGraph.right: " + rGraph.right + ", rGraph.bottom: " + rGraph.bottom);

        rLegendSrc = new Rect(0, 0, mLegend.getWidth(), mLegend.getHeight());
        rLegendDst = new Rect(rGraph.left, rGraph.bottom, rGraph.right, rGraphFrame.bottom);

        // SetSeekbarRanges();
        dBTrigger = -96.33f;
        minFreqTrigger = 16.f;
        maxFreqTrigger = 16000.f;
        setTriggerDB(-96.33f);
        setTriggerFreqs(16.f, 16000.f);
        bInitializedTriggerParams = true;
    }

    @Override protected void onDraw(Canvas canvas)
    {
        super.onDraw(canvas);
        if(bScrnTooSmall)return;
        float dB = renderGraph(mMainBitmap);
        if(bAccumulatingPeaks)
        {
            if(dB > fPeakDB)fPeakDB = dB;
        }
        canvas.drawBitmap(mMainBitmap, 0, 0, null);
        canvas.drawBitmap(mLegend, rLegendSrc, rLegendDst, null);
        canvas.drawRect(rGraphFrame.left, rGraphFrame.top, rGraphFrame.right, rGraphFrame.bottom, paintLine);
        if(bShowDBTriggerLine)
        {
            setTriggerDB(dBTrigger);
            if(Debug.ON)Log.d("onDraw","dBTrigger: " + dBTrigger);
            canvas.drawLine(rGraph.left, nTriggerDB_y, rGraph.right, nTriggerDB_y, paintLine);
            bShowDBTriggerLine = false;
        }else
        {
            canvas.drawLine(rGraph.left, nTriggerDB_y, rGraph.left + 16, nTriggerDB_y, paintLine);
            canvas.drawLine(rGraph.right - 16, nTriggerDB_y, rGraph.right, nTriggerDB_y, paintLine);
            canvas.drawLine(rGraph.left + 16, nTriggerDB_y, rGraph.right - 16, nTriggerDB_y, paintLineDim);
        }

        if(bMinFreqTriggerLine)
        {
            setTriggerFreqs(minFreqTrigger, maxFreqTrigger);
            fPeakDB = -100.f;
            canvas.drawLine(nTriggerMinFreq_x, rGraph.top, nTriggerMinFreq_x, rGraph.bottom, paintLine);
            bMinFreqTriggerLine = false;
        }else
        {
            canvas.drawLine(nTriggerMinFreq_x, rGraph.top, nTriggerMinFreq_x, rGraph.bottom, paintLineDim);
        }

        if(bMaxFreqTriggerLine)
        {
            setTriggerFreqs(minFreqTrigger, maxFreqTrigger);
            fPeakDB = -100.f;
            canvas.drawLine(nTriggerMaxFreq_x, rGraph.top, nTriggerMaxFreq_x, rGraph.bottom, paintLine);
            bMaxFreqTriggerLine = false;
        }else
        {
            canvas.drawLine(nTriggerMaxFreq_x, rGraph.top, nTriggerMaxFreq_x, rGraph.bottom, paintLineDim);
        }
/*
        if(bClearRecordingLamp)
        {
            canvas.drawRect((float) rRecordingLamp.left, (float)rRecordingLamp.top, (float) rRecordingLamp.right, (float) rRecordingLamp.bottom, paintRecordingLamp);
            bClearRecordingLamp = false;
        }
*/
    }
/*
    public void SetSeekbarRanges()
    {
       seekDB.setMax(rGraph.bottom - rGraph.top);
       seekDB.setProgress(seekDB.getMax());
       seekMinFreq.setMax((nTriggerMaxFreq_x - MINMAX_SEPARATION) - nTriggerMinFreq_x);
       seekMinFreq.setProgress(0);
       seekMaxFreq.setMax(nTriggerMaxFreq_x - (nTriggerMinFreq_x + MINMAX_SEPARATION));
       seekMaxFreq.setProgress(seekMaxFreq.getMax());
    }
*/
    public void AccumulatePeaks()
    {
        fPeakDB = -100.f;
        bAccumulatingPeaks = true;
    }

    public void StopAccumulatingPeaks()
    {
        fPeakDB = 0;
        bAccumulatingPeaks = false;
    }

    public float GetAccumulatedPeak()
    {
        return fPeakDB;
    }

    public int DBtoProgress(float dB)
    {
        int progress = -(int)(dB * dBscale);
        if(progress < 0)
        {
           progress = 0;
        }else if(progress > GetSeekDBRange())
        {
            progress = GetSeekDBRange();
        }
        return progress;
    }

 /*
    public float ProgressToDB(int progress)
    {
        float dB = -(float)progress / dBscale;
        if(dB < minDBTrigger)dB = minDBTrigger;
        return dB;
    }
*/
    public int FrequencyToMinFreqProgress(float fFrequency)
    {
        int progress, maxProgress = GetSeekMinFreqRange();
        progress = getGraphCoordAtFrequency(fFrequency) * 2;
        if(progress < 0)
        {
            progress = 0;
        }else if(progress > maxProgress)
        {
            progress = maxProgress;
        }
        return progress;
    }

    // Not sure this works for the general case
    // Only called once with a hard-coded frequency in PopSettings and it works for the current value there.
    public int FrequencyToMaxFreqProgress(float fFrequency)
    {
        int progress, maxProgress = GetSeekMaxFreqRange();
        progress = (getGraphCoordAtFrequency(fFrequency) * 2) - MINMAX_SEPARATION;
        if(progress < 0)
        {
            progress = 0;
        }else if(progress > maxProgress)
        {
            progress = maxProgress;
        }
        return progress;
    }

    public int GetSeekDBRange()
    {
        return (rGraph.bottom - rGraph.top);
    }

    public int GetSeekMinFreqRange()
    {
        return ((nInitialTriggerMaxFreq_x - MINMAX_SEPARATION) - nInitialTriggerMinFreq_x);
    }

    public int GetSeekMaxFreqRange()
    {
        return (nInitialTriggerMaxFreq_x - (nInitialTriggerMinFreq_x + MINMAX_SEPARATION));
    }


    public void SetDBTrigger(int progress)
    {
        // curTriggerParam = TRIGGER_PARAMS.SETDB;
        nTriggerDB_y = progress + rGraph.top;
        dBTrigger = -(float)progress / dBscale;
        if(dBTrigger < minDBTrigger)dBTrigger = minDBTrigger;
        // PopSettings.foo();
        // SetDBLabel(dBTrigger);
        bShowDBTriggerLine = true;
        if (MainActivity.ToggleOnOffState == MainActivity.ToggleState.OFF)
        {
            invalidate();
        }
    }

    public int GetDBTrigger()
    {
        return nTriggerDB_y - rGraph.top;
    }

    public int SetMinFreqTrigger(int progress)
    {
        int retval = 0;
        // curTriggerParam = TRIGGER_PARAMS.SETMINFREQ;
        nTriggerMinFreq_x = progress + nFreqTriggerMinX;
        if((nTriggerMinFreq_x + MINMAX_SEPARATION) > nTriggerMaxFreq_x)
        {
            // seekMaxFreq.setProgress(nTriggerMinFreq_x - nFreqTriggerMinX);
            retval = (nTriggerMinFreq_x - nFreqTriggerMinX);
        }
        minFreqTrigger = getFrequencyAtGraphCoord((nTriggerMinFreq_x - nFreqTriggerMinX) / 2);
        // SetMinFreqLabel(minFreqTrigger);
        bMinFreqTriggerLine = true;
        if (MainActivity.ToggleOnOffState == MainActivity.ToggleState.OFF)
        {
            invalidate();
        }
        return retval;
    }

    public int GetMinFreqTrigger()
    {
        return nTriggerMinFreq_x - nFreqTriggerMinX;
    }

    public int SetMaxFreqTrigger(int progress)
    {
        int retval = 0;
        // curTriggerParam = TRIGGER_PARAMS.SETMAXFREQ;
        nTriggerMaxFreq_x = progress + nFreqTriggerMinX + MINMAX_SEPARATION;
        if((nTriggerMaxFreq_x - MINMAX_SEPARATION) < nTriggerMinFreq_x)
        {
            // seekMinFreq.setProgress((nTriggerMaxFreq_x  - MINMAX_SEPARATION) - nFreqTriggerMinX);
            retval = ((nTriggerMaxFreq_x  - MINMAX_SEPARATION) - nFreqTriggerMinX);
        }
        maxFreqTrigger = getFrequencyAtGraphCoord((nTriggerMaxFreq_x - nFreqTriggerMinX) / 2);
        // SetMaxFreqLabel(maxFreqTrigger);
        bMaxFreqTriggerLine = true;
        if (MainActivity.ToggleOnOffState == MainActivity.ToggleState.OFF)
        {
            invalidate();
        }
        return retval;
    }

    public int GetMaxFreqTrigger()
    {
        return nTriggerMaxFreq_x - (nFreqTriggerMinX + MINMAX_SEPARATION);
    }

    public String GetDBLabel()
    {
        StringBuilder sbuf = new StringBuilder();
        Formatter fmt = new Formatter(sbuf);
        fmt.format("Sensitivity:  %.1f dB", dBTrigger);
        return sbuf.toString();
    }

    public String GetMinFreqLabel()
    {
        StringBuilder sbuf = new StringBuilder();
        Formatter fmt = new Formatter(sbuf);
        fmt.format("Min. frequency:  %.1f Hz", minFreqTrigger);
        return sbuf.toString();
    }

    public String GetMaxFreqLabel()
    {
        StringBuilder sbuf = new StringBuilder();
        Formatter fmt = new Formatter(sbuf);
        fmt.format("Max. frequency:  %.1f Hz", maxFreqTrigger);
        return sbuf.toString();
    }

    public void ResetTriggers()
    {
        minDBTrigger = -96.33f;
        minFreqTrigger = 16.f;
        maxFreqTrigger = 16000.f;
        nTriggerDB_y = rGraph.top + (int)(dBscale * 96.33f);
        // Log.v("onSizeChanged", "nTriggerDB_y: " + nTriggerDB_y);
        dBTrigger = -(float)(nTriggerDB_y - rGraph.top) / dBscale;
        // Log.v("onSizeChanged", "Default dBTrigger: " + dBTrigger);
        nTriggerMinFreq_x = nInitialTriggerMinFreq_x = nFreqTriggerMinX = rGraph.left + 3; // (These adjustments to avoid overwriting frame)
        // nTriggerMaxFreq_x = nInitialTriggerMaxFreq_x = nFreqTriggerMaxX = rGraph.right - 2;
        nTriggerMaxFreq_x = nInitialTriggerMaxFreq_x = rGraph.right - 2;
        bShowDBTriggerLine = true;
        bMinFreqTriggerLine = true;
        bMaxFreqTriggerLine = true;
        clearBitmap(mMainBitmap);
        invalidate();
    }

    public void ClearBitmap()
    {
       // mMainBitmap.eraseColor(getResources().getColor(R.color.colorTealAccent2)); // for testing only
       clearBitmap(mMainBitmap);
    }


    public void Redraw()
    {
        invalidate();
    }

    public void ClearScreen()
    {
        clearBitmap(mMainBitmap);
        invalidate();
    }
}