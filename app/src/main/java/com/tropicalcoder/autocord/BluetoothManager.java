package com.tropicalcoder.autocord;

import android.media.AudioDeviceInfo;
import android.media.AudioManager;
import android.util.Log;

import static android.media.AudioDeviceInfo.TYPE_BLUETOOTH_SCO;
import static android.media.AudioManager.GET_DEVICES_INPUTS;

class BluetoothManager
{
    final AudioManager audioManager;
    int bluetoothDeviceID;
    boolean bBluetoothConnected;

    BluetoothManager(AudioManager am)
    {
        audioManager = am;
        bBluetoothConnected = false;
        bluetoothDeviceID = -1;
    }

    public int GetDeviceID()
    {
        checkBluetoothConnected();
        if(bBluetoothConnected)
        {
            audioManager.setBluetoothScoOn(true);
            audioManager.startBluetoothSco();
        }
        return bluetoothDeviceID;
    }

    public void CloseConnection()
    {
        if(bBluetoothConnected)
        {
            audioManager.stopBluetoothSco();
            audioManager.setBluetoothScoOn(false);
            bBluetoothConnected = false;
            bluetoothDeviceID = -1;
        }
    }

    private void checkBluetoothConnected()
    {
        bBluetoothConnected = false;
        bluetoothDeviceID = -1;
        AudioDeviceInfo[] adi = audioManager.getDevices(GET_DEVICES_INPUTS);
        for(int i = 0; i < adi.length; i++)
        {
            int type = adi[i].getType();

            if(type == TYPE_BLUETOOTH_SCO)
            {
                Log.d("getDevices", "Device product name: " + adi[i].getProductName());
                Log.d("getDevices", "Device ID: " + adi[i].getId());
                bluetoothDeviceID = adi[i].getId();
                if(audioManager.isBluetoothScoAvailableOffCall())
                {
                    Log.d("getDevices","Bluetooth Sco Available OffCall");
                    bBluetoothConnected = true;
                }
            }
        }
    }

}

/*
        AudioDeviceInfo[] adi = audioManager.getDevices(GET_DEVICES_INPUTS);

        for(int i = 0; i < adi.length; i++)
        {
            Log.d("getDevices","Device product name: " + adi[i].getProductName());
            // Log.d("getAddress","Device product name: " + adi[i].getAddress()); // Call requires API 28

            Log.d("getDevices","Device ID: " + adi[i].getId());

            int type = adi[i].getType();

            switch(type)
            {
                case TYPE_BLUETOOTH_A2DP:
                    Log.d("getDevices","Device type: TYPE_BLUETOOTH_A2DP");
                    break;
                case TYPE_BLUETOOTH_SCO:
                    Log.d("getDevices","Device type: TYPE_BLUETOOTH_SCO");
                    bluetoothDeviceID = adi[i].getId();
                    if(audioManager.isBluetoothScoAvailableOffCall())
                    {
                        Log.d("getDevices","Bluetooth Sco Available OffCall");
                    }else
                    {
                        Log.d("getDevices","Bluetooth Sco NOT Available OffCall");
                    }
                    audioManager.setBluetoothScoOn(true);
                    audioManager.startBluetoothSco();
                    // audioManager.setMode(MODE_IN_COMMUNICATION);
                    break;
                case TYPE_BUILTIN_MIC:
                    Log.d("getDevices","Device type: TYPE_BUILTIN_MIC");
                    break;
                case TYPE_UNKNOWN:
                    Log.d("getDevices","Device type: TYPE_UNKNOWN");
                    break;

                case TYPE_LINE_DIGITAL:
                    Log.d("getDevices","Device type: TYPE_LINE_DIGITAL");
                    break;
                case TYPE_LINE_ANALOG:
                    Log.d("getDevices","Device type: TYPE_UNKNOWN");
                    break;
                case TYPE_TELEPHONY:
                    Log.d("getDevices","Device type: TYPE_TELEPHONY");
                    break;
                default:
                    Log.d("getDevices","Device type: " + type);
                    break;
            }


            int[] sampleRates = adi[i].getSampleRates();
            if(sampleRates.length == 0)
            {
                Log.d("getSampleRates", "Device supports arbitrary rates");
            }else
            {
                for (int j = 0; j < sampleRates.length; j++)
                {
                    Log.d("getSampleRates", "Sample rate: " + sampleRates[j]);
                }
            }

            int[] channelCounts = adi[i].getChannelCounts();
            if(channelCounts.length == 0)
            {
                Log.d("getChannelCounts","Device supports arbitrary channel counts");
            }else
            {
                for (int q = 0; q < channelCounts.length; q++)
                {
                    Log.d("getChannelCounts", "Channel counts: " + channelCounts[q]);
                }
            }
        }

 */