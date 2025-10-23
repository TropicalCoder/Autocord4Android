package com.tropicalcoder.autocord;


import android.app.Activity;
import android.os.Bundle;
import android.os.Handler;
import android.util.DisplayMetrics;
import android.util.Log;
import android.view.Gravity;
import android.view.View;
import android.view.WindowManager;
import android.widget.ImageButton;
import android.widget.SeekBar;
import android.widget.TextView;

import static android.content.ContentValues.TAG;


public class PopSettings extends Activity
{
    public enum PRESETS{VOICE, BIRDS, ANY}

    private float fMinInitialFrequency = 320.f;
    private float fMaxInitialFrequency = 4000.f;
    public static final int STOP_TIMER = 2147483647;
    private final Handler mTimerHandler = new Handler();
    private boolean bTimerStopping = false;
    private boolean bAutoAdjust = false;
    private boolean bAdjustFrequencyTriggers = true;

    public ImageButton btnAutoAdjust;
    public ImageButton btnCancelAutoAdjust;
    public TextView labelAdjust;
    public TextView labSetDB;
    public TextView labSetMinFreq;
    public TextView labSetMaxFreq;
    public SeekBar seekDB;
    public SeekBar seekMinFreq;
    public SeekBar seekMaxFreq;

    @Override
    protected void onCreate(Bundle savedInstance)
    {
        super.onCreate(savedInstance);
        setContentView(R.layout.popwindow);

        WindowManager.LayoutParams params = getWindow().getAttributes();
        params.gravity = Gravity.BOTTOM;

        DisplayMetrics dm = new DisplayMetrics();
        getWindowManager().getDefaultDisplay().getMetrics(dm);

        getWindow().setLayout(dm.widthPixels, params.height);

        if(MainActivity.triggerPreset == PRESETS.VOICE)
        {
            fMinInitialFrequency = 320.f;
            fMaxInitialFrequency = 4000.f;
        }else if(MainActivity.triggerPreset == PRESETS.BIRDS)
        {
            fMinInitialFrequency = 3000.f;
            fMaxInitialFrequency = 5000.f;
        }else if(MainActivity.triggerPreset == PRESETS.ANY)
        {
            fMinInitialFrequency = 90.f;
            fMaxInitialFrequency = 8000.f;
        }
        seekDB = (SeekBar)findViewById(R.id.seekDB);
        seekDB.setMax(MainActivity.customCanvas.GetSeekDBRange());
        seekDB.setProgress(MainActivity.customCanvas.GetDBTrigger());
        seekDB.setOnSeekBarChangeListener(onSeekBarChangeListener);
        seekMinFreq = (SeekBar)findViewById(R.id.seekMinFreq);
        seekMinFreq.setMax(MainActivity.customCanvas.GetSeekMinFreqRange());
        seekMinFreq.setProgress(MainActivity.customCanvas.GetMinFreqTrigger());
        seekMinFreq.setOnSeekBarChangeListener(onSeekBarChangeListener);
        seekMaxFreq = (SeekBar)findViewById(R.id.seekMaxFreq);
        seekMaxFreq.setMax(MainActivity.customCanvas.GetSeekMaxFreqRange());
        seekMaxFreq.setProgress(MainActivity.customCanvas.GetMaxFreqTrigger());
        seekMaxFreq.setOnSeekBarChangeListener(onSeekBarChangeListener);
        labSetDB = (TextView) findViewById(R.id.labelSeekDB);
        labSetMinFreq = (TextView) findViewById(R.id.labelSeekMinFreq);
        labSetMaxFreq = (TextView) findViewById(R.id.labelSeekMaxFreq);
        labSetDB.setText(MainActivity.customCanvas.GetDBLabel());
        labSetMinFreq.setText(MainActivity.customCanvas.GetMinFreqLabel());
        labSetMaxFreq.setText(MainActivity.customCanvas.GetMaxFreqLabel());
        labelAdjust = (TextView) findViewById(R.id.labelAdjust);
        // Sound effects or haptic feedback on auto button generate a bump
        // in the spectrum that kicks the trigger sensitivity line up - so disable them.
        btnAutoAdjust = (ImageButton)findViewById(R.id.imageBtnAutoOn);
        btnAutoAdjust.setHapticFeedbackEnabled(false);
        btnAutoAdjust.setSoundEffectsEnabled(false);
        btnCancelAutoAdjust = (ImageButton)findViewById((R.id.imageBtnAutoOff));
        btnCancelAutoAdjust.setHapticFeedbackEnabled(false);
        btnCancelAutoAdjust.setSoundEffectsEnabled(false);
    }

    private void setTimer(int delayMillis)
    {
        bTimerStopping = true;
        mTimerHandler.removeCallbacks(mTimerRunnable);
        if(delayMillis != STOP_TIMER)
        {
            // Log.v(TAG, "Set timer!");
            bTimerStopping = false;
            mTimerHandler.postDelayed(mTimerRunnable, delayMillis);
            // Log.v(TAG, "mTimerHandler.postDelayed");
        }else if(Debug.ON)
        {
            Log.v(TAG, "Popup killed timer!");
        }
    }

    private final Runnable mTimerRunnable = new Runnable()
    {
        @Override
        public void run()
        {
            float dB = MainActivity.customCanvas.GetAccumulatedPeak();
            // Add 3 dB of headroom
            dB += 3;
            // Log.v(TAG, "dB: " + dB);
            int progress = MainActivity.customCanvas.DBtoProgress(dB);
            seekDB.setProgress(progress);
            // Log.v(TAG, "Progress: " + seekDB.getProgress());

            if(!bTimerStopping)
            {
               setTimer(200);
            }
        }
    };

    public void onToggleAuto(View view)
    {
        if(MainActivity.isCapturingAudio())
        {
            bAutoAdjust = !bAutoAdjust;
            if (bAutoAdjust)
            {
                if(bAdjustFrequencyTriggers)
                {
                    int progress = MainActivity.customCanvas.FrequencyToMinFreqProgress(fMinInitialFrequency);
                    seekMinFreq.setProgress(progress);

                    progress = MainActivity.customCanvas.FrequencyToMaxFreqProgress(fMaxInitialFrequency);
                    seekMaxFreq.setProgress(progress);
                }

                MainActivity.customCanvas.AccumulatePeaks();
                setTimer(200);
                btnAutoAdjust.setVisibility(View.INVISIBLE);
                btnCancelAutoAdjust.setVisibility(View.VISIBLE);
            } else
            {
                MainActivity.customCanvas.StopAccumulatingPeaks();
                setTimer(STOP_TIMER);
                btnAutoAdjust.setVisibility(View.VISIBLE);
                btnCancelAutoAdjust.setVisibility(View.INVISIBLE);
            }
        } // End if(MainActivity.isCapturingAudio())
    }

    @Override
    public void onResume()
    {
        super.onResume();
        if(!MainActivity.isCapturingAudio())
        {
            // Take away the label to adjust and the button
            labelAdjust.setVisibility(View.INVISIBLE);
            btnAutoAdjust.setVisibility(View.INVISIBLE);
        }
    }

    @Override
    public void onPause()
    {
        super.onPause();
        if(MainActivity.isCapturingAudio())
        {
            MainActivity.customCanvas.StopAccumulatingPeaks();
            setTimer(STOP_TIMER);
        }
    }

    public void onClose(View view)
    {
        if(MainActivity.isCapturingAudio())
        {
            MainActivity.customCanvas.StopAccumulatingPeaks();
            setTimer(STOP_TIMER);
        }
        finish();
    }

    final SeekBar.OnSeekBarChangeListener onSeekBarChangeListener = new SeekBar.OnSeekBarChangeListener()
    {
        public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser)
        {
            if (seekBar == seekDB)
            {
                MainActivity.customCanvas.SetDBTrigger(progress);
                labSetDB.setText(MainActivity.customCanvas.GetDBLabel());
            } else if (seekBar == seekMinFreq)
            {
                int retval = MainActivity.customCanvas.SetMinFreqTrigger(progress);
                if(retval > 0)
                {
                    // Seems we never enter here!
                    seekMaxFreq.setProgress(retval);
                }
                // If user adjusts the frequency triggers, then goes for autoAdjustMode
                // within the same session, respect the user's frequency trigger settings
                // and don't change them (bAdjustFrequencyTriggers = false).
                bAdjustFrequencyTriggers = false;
                labSetMinFreq.setText(MainActivity.customCanvas.GetMinFreqLabel());
            } else // seekBar == seekMaxFreq
            {
                int retval = MainActivity.customCanvas.SetMaxFreqTrigger(progress);
                if(retval > 0)
                {
                    // Seems we never enter here!
                    seekMinFreq.setProgress(retval);
                }
                bAdjustFrequencyTriggers = false;
                labSetMaxFreq.setText(MainActivity.customCanvas.GetMaxFreqLabel());
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
