package com.tropicalcoder.autocord;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Intent;
import android.os.Binder;
import android.os.IBinder;
import android.util.Log;

import androidx.core.app.NotificationCompat;

import static android.content.ContentValues.TAG;

//import android.support.v4.app.NotificationCompat;

public class AudioService extends Service
{
    // Unique Notification ID
    private static final int NOTIFICATION_ID = 1001;
    private static final String NOTIFICATION_CHANNEL_ID = "my_notification_channel";

    // Custom Binder
    private final MyBinder mLocalbinder = new MyBinder();

    // Callback Setter
    public void setCallBack(CallBack callBack)
    {
        // Custom interface Callback which is declared in this Service
    }

    public interface CallBack {
        void onOperationProgress(int progress);
        void onOperationCompleted(int user);
    }

    // The binding is asynchronous, and bindService() returns immediately without returning the IBinder to the client.
    // To receive the IBinder, the client must create an instance of ServiceConnection and pass it to bindService().
    // The ServiceConnection includes a callback method that the system calls to deliver the IBinder.
    //Custom Binder class
    class MyBinder extends Binder
    {
        AudioService getService()
        {
            return AudioService.this;
        }
    }

    private Notification getNotification()
    {
        NotificationManager notificationManager = (NotificationManager) getSystemService(NOTIFICATION_SERVICE);
        assert notificationManager != null;

        if (notificationManager.getNotificationChannel(NOTIFICATION_CHANNEL_ID) == null)
        {
            NotificationChannel notificationChannel = new NotificationChannel(NOTIFICATION_CHANNEL_ID, "My Notifications", NotificationManager.IMPORTANCE_DEFAULT);

            // Configure the notification channel.
            //notificationChannel.setDescription("Channel description");
            //notificationChannel.enableLights(true);
           // notificationChannel.setLightColor(Color.RED);
            // notificationChannel.setVibrationPattern(new long[]{0, 1000, 500, 1000});
            // notificationChannel.enableVibration(true); // was this
            notificationChannel.enableVibration(false); // new
            notificationChannel.setSound(null, null);
            notificationManager.createNotificationChannel(notificationChannel);
        }else
        {
            if(Debug.ON)Log.v(TAG, "Notification channel was already registered!");
        }

        Intent notificationIntent = new Intent(this, MainActivity.class);
        PendingIntent pendingIntent = PendingIntent.getActivity(this, 0, notificationIntent, 0);

        NotificationCompat.Builder builder = new NotificationCompat.Builder(this, NOTIFICATION_CHANNEL_ID)
                .setContentIntent(pendingIntent)
                //.setVibrate(new long[]{0, 100, 100, 100, 100, 100})
                //.setSound(RingtoneManager.getDefaultUri(RingtoneManager.TYPE_NOTIFICATION))
                .setSound(null)
                .setVibrate(null)
                .setContentTitle("Autocord")
                .setContentText("Running")
                .setSmallIcon(R.drawable.ic_equalizer);
        // notificationManager.notify(NOTIFICATION_ID, builder.build()); don't do this 'cause will be done by startForeground()
        return builder.build();
    }

/*
    @Override
    public void onCreate()
    {
        super.onCreate();
    }
*/
    @Override
    public int onStartCommand(Intent intent, int flags, int startID)
    {
        if(Debug.ON)Log.v(TAG, "flags: " +  flags + ", startID: " + startID);
        // if(startID == 1) seems to increment from 1 fo each time the notification is touched
        // Was experimenting here to avoid restarting service if already running by doing...
        // if(startID == 1) startForeground(NOTIFICATION_ID, getNotification());
        // However, MainActivity is restarted anyhow if not this service
        startForeground(NOTIFICATION_ID, getNotification());
        return START_STICKY;
    }

/*
    @Override
    public void onDestroy()
    {
        super.onDestroy();
    }
*/
    @Override
    public IBinder onBind(Intent intent)
    {
        // Getting the instance of Binder
        return mLocalbinder;
    }
}
