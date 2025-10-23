package com.tropicalcoder.autocord;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;

class BroadcastRcvr extends BroadcastReceiver
{
    @Override
    public void onReceive(Context context, Intent intent)
    {
        String sAction = intent.getAction();
        if(sAction != null)
        {
            if (sAction.contains("SCREEN_ON"))
            {
                // android.intent.action.SCREEN_ON
                // Log.d(TAG, "My broadcast receiver says screen is on!");
                MainActivity.bScreenIsOn = true;
            } else if (sAction.contains("SCREEN_OFF"))
            {
                // Log.d(TAG, "My broadcast receiver says screen is off!");
                MainActivity.bScreenIsOn = false;
            }
        }
    }
}
