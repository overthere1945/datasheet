/*============================================================================
  Copyright (c) 2017-2021, 2023-2024 Qualcomm Technologies, Inc.
  All Rights Reserved.
  Confidential and Proprietary - Qualcomm Technologies, Inc.

  @file SensorRequestFragment.java
  @brief
   SensorRequestFragment class implementation
============================================================================*/
package com.qualcomm.qti.usta.ui;

import android.app.Fragment;
import android.app.FragmentManager;
import android.content.Context;
import android.os.Bundle;
import android.text.Editable;
import android.text.TextWatcher;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.View;
import android.view.ViewGroup;
import android.view.inputmethod.InputMethodManager;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.Spinner;
import android.widget.TextView;
import android.widget.Toast;

import com.qualcomm.qti.usta.R;
import com.qualcomm.qti.usta.core.AccessModifier;
import com.qualcomm.qti.usta.core.DataType;
import com.qualcomm.qti.usta.core.SensorContextJNI;
import com.qualcomm.qti.usta.core.SensorPayloadField;
import com.qualcomm.qti.usta.core.SensorScriptUtility;
import com.qualcomm.qti.usta.core.USTALog;
import com.qualcomm.qti.usta.core.ModeType;
import com.qualcomm.qti.usta.core.SensorContext;

import java.util.Arrays;
import java.util.Iterator;
import java.util.Map;
import java.util.Vector;


public class SensorRequestFragment extends Fragment {

  private SensorContext sensorContext;
  private View sensorRequestFragmentView;

  private int sensorHandle;
  private int sensorReqHandle;
  private CheckBox flushOnlyCB;
  private CheckBox maxBatchCB;
  private CheckBox isPassiveCB;
  private Spinner sensorListSpinner;
  private Spinner sensorReqMsgSpinner;
  private Spinner preDefiniedScriptSpinner;
  private ArrayAdapter<String> sensorReqMsgSpinnerAdapter;
  private Spinner clientProcessorSpinner;
  private Spinner wakeupDeliverySpinner;
  private EditText batchPeriodEditText;
  private EditText flushPeriodEditText;
  public static int previousSpinnerPosition = -1;
  private CheckBox resamplerConfigCB;
  private Spinner resamplerTypeSpinner;
  private CheckBox resamplerFilterCB;
  private CheckBox thresholdConfigCB;
  private Spinner thresholdTypeSpinner;
  private EditText thresholdValueXAxisEditText;
  private EditText thresholdValueYAxisEditText;
  private EditText thresholdValueZAxisEditText;
  private Spinner clientConnectIdSpinner;

  @Override
  public void onCreate(Bundle savedInstanceData) {
    super.onCreate(savedInstanceData);
    sensorContext = SensorContext.getSensorContext(ModeType.USTA_MODE_TYPE_UI);
    setHasOptionsMenu(true);
  }

  @Override
  public void onPrepareOptionsMenu(Menu menu) {
    menu.findItem(R.id.disable_qmi_connection_id).setEnabled(false);
  }

  private void setupSensorListSpinner() {

    sensorListSpinner = (Spinner) sensorRequestFragmentView.findViewById(R.id.sensor_list_spinner);
    ArrayAdapter<String> sensorListSpinnerAdapter = new ArrayAdapter<>(getActivity().getApplicationContext(), android.R.layout.simple_list_item_1, sensorContext.getSensorNames());

    sensorListSpinner.setAdapter(sensorListSpinnerAdapter);

    sensorListSpinner.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
      @Override
      public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {

        if(SensorEventDisplayFragment.isFirstTimeThreadCreation == false) {
          SensorEventDisplayFragment.destroyThread();
        }
        SensorContextJNI.disableStreamingStatusNativeWrapper(previousSpinnerPosition);
        sensorHandle = sensorListSpinner.getSelectedItemPosition();
        previousSpinnerPosition = sensorHandle;
        SensorEventDisplayFragment.updateCurrentSensorHandle(sensorHandle);
        /*Start new Thread to get the pull data from native*/
        SensorEventDisplayFragment.startThread();
        SensorContextJNI.enableStreamingStatusNativeWrapper(sensorHandle);

        TextView suidLowTextView = (TextView) sensorRequestFragmentView.findViewById(R.id.suid_low);
        suidLowTextView.setText(sensorContext.getSensors().get(sensorHandle).getSensorSUIDLow());

        TextView suidHighTextView = (TextView) sensorRequestFragmentView.findViewById(R.id.suid_high);
        suidHighTextView.setText(sensorContext.getSensors().get(sensorHandle).getSensorSUIDHigh());

        TextView onChangeTextView = (TextView) sensorRequestFragmentView.findViewById(R.id.on_change_value);
        if(true == sensorContext.getSensors().get(sensorHandle).isOnChangeSensor()) {
          onChangeTextView.setText("True");
        } else {
          onChangeTextView.setText("False");
        }

        TextView hwIDTextView = (TextView) sensorRequestFragmentView.findViewById(R.id.hw_id_value);
        int hwID = sensorContext.getSensors().get(sensorHandle).getHwID();
        if(hwID == -1) {
          hwIDTextView.setText("NA");
        } else {
          hwIDTextView.setText(Integer.toString(hwID));
        }

        TextView rigitBodyTypeView = (TextView) sensorRequestFragmentView.findViewById(R.id.rigid_body_type);
        int rigidBodyType = sensorContext.getSensors().get(sensorHandle).getRigidBodyType();
        if(rigidBodyType == -1) {
          rigitBodyTypeView.setText("NA");
        } else {
          rigitBodyTypeView.setText(Integer.toString(rigidBodyType));
        }

        TextView sensorNameView = (TextView) sensorRequestFragmentView.findViewById(R.id.sensor_name);

        String sensorName = sensorContext.getSensors().get(sensorHandle).getSensorName();
        if(sensorName == "") {
          sensorNameView.setText("NA");
        } else {
          sensorNameView.setText(sensorName);
        }

        if(sensorContext.getSensorNames().get(sensorHandle).contains("registry")) {
          SensorContext.throwErrorDialog("Registry functionality is implemented in Registry TAB.  Please check in another Tab.", getContext());
        }
        else{
          resetRequestMsgListSpinner();
          resetPreDefinedScriptsSpinner();
          Toast.makeText(getActivity().getApplicationContext(), "You have selected: " + sensorContext.getSensorNames().get(sensorHandle), Toast.LENGTH_SHORT).show();
        }

      }

      @Override
      public void onNothingSelected(AdapterView<?> parent) {

      }
    });
  }

  private void assignStaticDataFromScript(Map scriptDataTable){
    /*Set Req Msg ID */
    sensorReqMsgSpinner = (Spinner) sensorRequestFragmentView.findViewById(R.id.req_msg_list_spinner);
    if(scriptDataTable != null && scriptDataTable.get("Req_Msgs_ID") != null) {
      Vector<String> req_MsgID = (Vector<String>)scriptDataTable.get("Req_Msgs_ID");
      Vector<String> reqMsgIDList = sensorContext.getSensors().get(sensorHandle).getSensorReqMsgsNamesWithMsgID();

      int defaultSpinnerPosition = 0;
      for(int reqMsgIndex = 0 ; reqMsgIndex < reqMsgIDList.size() ; reqMsgIndex++){
        if(reqMsgIDList.elementAt(reqMsgIndex).contains("513")) {
          defaultSpinnerPosition = reqMsgIndex;
          break;
        }
      }
      if (req_MsgID == null || req_MsgID.elementAt(0) == null || req_MsgID.elementAt(0).length() == 0 || req_MsgID.elementAt(0).equals("null")){
        sensorReqMsgSpinner.setSelection(defaultSpinnerPosition); // Default value for spinner
      }else {
        for(int loopIndex = 0 ; loopIndex < reqMsgIDList.size() ; loopIndex++){
          if(reqMsgIDList.elementAt(loopIndex).contains(req_MsgID.elementAt(0)) == true) {
            sensorReqMsgSpinner.setSelection(loopIndex);
            defaultSpinnerPosition = loopIndex;
            break;
          }
        }
      }
      sensorReqHandle = defaultSpinnerPosition;
    }
    /*Set Client Processor*/
    clientProcessorSpinner = (Spinner) sensorRequestFragmentView.findViewById(R.id.client_processor_spinner);
    if(scriptDataTable != null && scriptDataTable.get("Client_Processor") != null) {
      Vector<String> client_processor = (Vector<String>)scriptDataTable.get("Client_Processor");
      USTALog.d("client_processor from script is " + client_processor.elementAt(0).toLowerCase());
      if (client_processor == null || client_processor.elementAt(0) == null || client_processor.elementAt(0).length() == 0 || client_processor.elementAt(0).equals("null")){
        clientProcessorSpinner.setSelection(0); // Default value for spinner
        sensorContext.getSensors().get(sensorHandle).getSensorStdFields(sensorReqHandle).setClientProcessor(0);
      }else {
        Vector<String> clientProcessorList = sensorContext.getClientProcessors();
        for(int loopIndex = 0 ; loopIndex < clientProcessorList.size() ; loopIndex++){
          USTALog.d("client_processor from list is " + clientProcessorList.elementAt(loopIndex));
          if(clientProcessorList.elementAt(loopIndex).equals(client_processor.elementAt(0).toLowerCase()) == true) {
            USTALog.d("setting client_processor from list is " + clientProcessorList.elementAt(loopIndex) + " as spiiner position " + loopIndex);
            clientProcessorSpinner.setSelection(loopIndex);
            sensorContext.getSensors().get(sensorHandle).getSensorStdFields(sensorReqHandle).setClientProcessor(loopIndex);
            break;
          }
        }
      }
    }
    /*Set Wakeup Type */
    wakeupDeliverySpinner = (Spinner) sensorRequestFragmentView.findViewById(R.id.wakeup_spinner);
    if(scriptDataTable != null && scriptDataTable.get("wakeup_delivery") != null) {
      Vector<String> wakeup_delivery = (Vector<String>)scriptDataTable.get("wakeup_delivery");
      USTALog.d("wakeup_delivery from script is " + wakeup_delivery.elementAt(0).toLowerCase());
      if (wakeup_delivery == null || wakeup_delivery.elementAt(0) == null || wakeup_delivery.elementAt(0).length() == 0 || wakeup_delivery.elementAt(0).equals("null")){
        wakeupDeliverySpinner.setSelection(0); // Default value for spinner
        sensorContext.getSensors().get(sensorHandle).getSensorStdFields(sensorReqHandle).setWakeupDevilvery(0);
      }else {
        Vector<String> wakeDeliverList = sensorContext.getWakeupDelivery();
        for(int loopIndex = 0 ; loopIndex < wakeDeliverList.size() ; loopIndex++){
          USTALog.d("wakeDeliver from list is " + wakeDeliverList.elementAt(loopIndex));
          if(wakeDeliverList.elementAt(loopIndex).equals(wakeup_delivery.elementAt(0).toLowerCase()) == true) {
            USTALog.d("setting wakeDeliver from list is " + wakeDeliverList.elementAt(loopIndex) + " as spiiner position " + loopIndex);
            wakeupDeliverySpinner.setSelection(loopIndex);
            sensorContext.getSensors().get(sensorHandle).getSensorStdFields(sensorReqHandle).setWakeupDevilvery(loopIndex);
            break;
          }
        }
      }
    }

    /*Set Batch Period  */
    batchPeriodEditText = (EditText) sensorRequestFragmentView.findViewById(R.id.batch_period_input_text);
    if(scriptDataTable != null && scriptDataTable.get("batch_period") != null) {
      Vector<String> batch_period = (Vector<String>)scriptDataTable.get("batch_period");
      if (batch_period == null || batch_period.elementAt(0) == null || batch_period.elementAt(0).length() == 0 || batch_period.elementAt(0).equals("null")){
        batchPeriodEditText.setText(null);
        sensorContext.getSensors().get(sensorHandle).getSensorStdFields(sensorReqHandle).setBatchPeriod(null);
      }else {
        batchPeriodEditText.setText(batch_period.elementAt(0));
        sensorContext.getSensors().get(sensorHandle).getSensorStdFields(sensorReqHandle).setBatchPeriod(batch_period.elementAt(0));
      }
    }
    /*Set Flush period */
    flushPeriodEditText = (EditText) sensorRequestFragmentView.findViewById(R.id.flush_period_input_text);
    if(scriptDataTable != null && scriptDataTable.get("flush_period") != null) {
      Vector<String> flush_period = (Vector<String>)scriptDataTable.get("flush_period");
      if (flush_period == null || flush_period.elementAt(0) == null || flush_period.elementAt(0).length() == 0 || flush_period.elementAt(0).equals("null")){
        flushPeriodEditText.setText(null);
        sensorContext.getSensors().get(sensorHandle).getSensorStdFields(sensorReqHandle).setFlushPeriod(null);
      }else {
        flushPeriodEditText.setText(flush_period.elementAt(0));
        sensorContext.getSensors().get(sensorHandle).getSensorStdFields(sensorReqHandle).setFlushPeriod(flush_period.elementAt(0));
      }
    }
    /*Set Flush Only*/
    flushOnlyCB = (CheckBox) sensorRequestFragmentView.findViewById(R.id.flush_only_checkbox);
    if(scriptDataTable != null && scriptDataTable.get("flush_only") != null) {
      Vector<String> flush_only = (Vector<String>)scriptDataTable.get("flush_only");
      if (flush_only == null || flush_only.elementAt(0) == null || flush_only.elementAt(0).length() == 0 || flush_only.elementAt(0).equals("false") || flush_only.elementAt(0).equals("null")){
        flushOnlyCB.setChecked(false);
        sensorContext.getSensors().get(sensorHandle).getSensorStdFields(sensorReqHandle).setFlushOnly(false);
      }else {
        flushOnlyCB.setChecked(true);
        sensorContext.getSensors().get(sensorHandle).getSensorStdFields(sensorReqHandle).setFlushOnly(true);
      }
    }
    /*Set Max Batch */
    maxBatchCB = (CheckBox) sensorRequestFragmentView.findViewById(R.id.max_batch_checkbox);
    if(scriptDataTable != null && scriptDataTable.get("max_batch") != null) {
      Vector<String> bax_batch = (Vector<String>)scriptDataTable.get("max_batch");
      if (bax_batch == null || bax_batch.elementAt(0) == null || bax_batch.elementAt(0).length() == 0 || bax_batch.elementAt(0).equals("false") || bax_batch.elementAt(0).equals("null")){
        maxBatchCB.setChecked(false);
        sensorContext.getSensors().get(sensorHandle).getSensorStdFields(sensorReqHandle).setMaxBatch(false);
      }else {
        maxBatchCB.setChecked(true);
        sensorContext.getSensors().get(sensorHandle).getSensorStdFields(sensorReqHandle).setMaxBatch(true);
      }
    }

    isPassiveCB = (CheckBox) sensorRequestFragmentView.findViewById(R.id.is_passive_checkbox);
    if(scriptDataTable != null && scriptDataTable.get("is_passive") != null) {
      Vector<String> is_passive = (Vector<String>)scriptDataTable.get("is_passive");
      if (is_passive == null || is_passive.elementAt(0) == null || is_passive.elementAt(0).length() == 0 || is_passive.elementAt(0).equals("false") || is_passive.elementAt(0).equals("null")){
        isPassiveCB.setChecked(false);
        sensorContext.getSensors().get(sensorHandle).getSensorStdFields(sensorReqHandle).setPassive(false);
      }else {
        isPassiveCB.setChecked(true);
        sensorContext.getSensors().get(sensorHandle).getSensorStdFields(sensorReqHandle).setPassive(true);
      }
    }

    resamplerConfigCB = (CheckBox) sensorRequestFragmentView.findViewById(R.id.is_resample_checkbox);
    if(scriptDataTable != null && scriptDataTable.get("is_resample") != null) {
      Vector<String> isResampled = (Vector<String>)scriptDataTable.get("is_resample");
      if (isResampled == null || isResampled.elementAt(0) == null || isResampled.elementAt(0).length() == 0 || isResampled.elementAt(0).equals("false") || isResampled.elementAt(0).equals("null")){
        resamplerConfigCB.setChecked(false);
        sensorContext.getSensors().get(sensorHandle).getSensorStdFields(sensorReqHandle).setResampled(false);
      }else {
        resamplerConfigCB.setChecked(true);
        sensorContext.getSensors().get(sensorHandle).getSensorStdFields(sensorReqHandle).setResampled(true);
      }
    }
    USTALog.i("assignStaticDataFromScript  is_resample Done");

    resamplerTypeSpinner = (Spinner) sensorRequestFragmentView.findViewById(R.id.resampler_type_spinner_id);
    if(scriptDataTable != null && scriptDataTable.get("resampler_type") != null) {
      Vector<String> resamplerType = (Vector<String>)scriptDataTable.get("resampler_type");
      USTALog.d("client_processor from script is " + resamplerType.elementAt(0).toLowerCase());
      if (resamplerType == null || resamplerType.elementAt(0) == null || resamplerType.elementAt(0).length() == 0 || resamplerType.elementAt(0).equals("null")){
        resamplerTypeSpinner.setSelection(0);
        sensorContext.getSensors().get(sensorHandle).getSensorStdFields(sensorReqHandle).setResamplerType(0);
      }else {
        Vector<String> resamplerTypeList = sensorContext.getResamplerTypes();
        for(int loopIndex = 0 ; loopIndex < resamplerTypeList.size() ; loopIndex++){
          USTALog.d("resamplerTypeList from list is " + resamplerTypeList.elementAt(loopIndex));
          if(resamplerTypeList.elementAt(loopIndex).equals(resamplerType.elementAt(0).toLowerCase()) == true) {
            USTALog.d("setting resamplerType from list is " + resamplerTypeList.elementAt(loopIndex) + " as spinner position " + loopIndex);
            resamplerTypeSpinner.setSelection(loopIndex);
            sensorContext.getSensors().get(sensorHandle).getSensorStdFields(sensorReqHandle).setResamplerType(loopIndex);
            break;
          }
        }
      }
    }
    USTALog.i("assignStaticDataFromScript  resampler_type Done");

    resamplerFilterCB = (CheckBox) sensorRequestFragmentView.findViewById(R.id.resampler_filter_checkBox);
    if(scriptDataTable != null && scriptDataTable.get("resampler_filter") != null) {
      Vector<String> resamplerFilter = (Vector<String>)scriptDataTable.get("resampler_filter");
      if (resamplerFilter == null || resamplerFilter.elementAt(0) == null || resamplerFilter.elementAt(0).length() == 0 || resamplerFilter.elementAt(0).equals("false") || resamplerFilter.elementAt(0).equals("null")){
        resamplerFilterCB.setChecked(false);
        sensorContext.getSensors().get(sensorHandle).getSensorStdFields(sensorReqHandle).setResamplerFilter(false);
      }else {
        resamplerFilterCB.setChecked(true);
        sensorContext.getSensors().get(sensorHandle).getSensorStdFields(sensorReqHandle).setResamplerFilter(true);
      }
    }
    USTALog.i("assignStaticDataFromScript  resampler_filter Done");

    thresholdConfigCB = (CheckBox) sensorRequestFragmentView.findViewById(R.id.is_threshold_checkBox);
    if(scriptDataTable != null && scriptDataTable.get("is_threshold") != null) {
      Vector<String> isThreshold = (Vector<String>)scriptDataTable.get("is_threshold");
      if (isThreshold == null || isThreshold.elementAt(0) == null || isThreshold.elementAt(0).length() == 0 || isThreshold.elementAt(0).equals("false") || isThreshold.elementAt(0).equals("null")){
        thresholdConfigCB.setChecked(false);
        sensorContext.getSensors().get(sensorHandle).getSensorStdFields(sensorReqHandle).setThresholded(false);
      }else {
        thresholdConfigCB.setChecked(true);
        sensorContext.getSensors().get(sensorHandle).getSensorStdFields(sensorReqHandle).setThresholded(true);
      }
    }
    USTALog.i("assignStaticDataFromScript  is_threshold Done");
    thresholdTypeSpinner = (Spinner) sensorRequestFragmentView.findViewById(R.id.threshold_type_spinner_id);
    if(scriptDataTable != null && scriptDataTable.get("threshold_type") != null) {
      Vector<String> thresholdType = (Vector<String>)scriptDataTable.get("threshold_type");
      USTALog.d("threshold_type from script is " + thresholdType.elementAt(0).toLowerCase());
      if (thresholdType == null || thresholdType.elementAt(0) == null || thresholdType.elementAt(0).length() == 0 || thresholdType.elementAt(0).equals("null")){
        thresholdTypeSpinner.setSelection(0); // Default value for spinner
        sensorContext.getSensors().get(sensorHandle).getSensorStdFields(sensorReqHandle).setThresholdType(0);
      }else {
        Vector<String> thresholdTypeList = sensorContext.getThresholdTypes();
        for(int loopIndex = 0 ; loopIndex < thresholdTypeList.size() ; loopIndex++){
          USTALog.d("resamplerTypeList from list is " + thresholdTypeList.elementAt(loopIndex));
          if(thresholdTypeList.elementAt(loopIndex).equals(thresholdType.elementAt(0).toLowerCase()) == true) {
            USTALog.d("setting thresholdType from list is " + thresholdTypeList.elementAt(loopIndex) + " as spinner position " + loopIndex);
            thresholdTypeSpinner.setSelection(loopIndex);
            sensorContext.getSensors().get(sensorHandle).getSensorStdFields(sensorReqHandle).setThresholdType(loopIndex);
            break;
          }
        }
      }
    }
    USTALog.i("assignStaticDataFromScript  threshold_type Done");
    thresholdValueXAxisEditText = (EditText) sensorRequestFragmentView.findViewById(R.id.threshold_value_axis_1);
    if(scriptDataTable != null && scriptDataTable.get("threshold_value_x_axis") != null) {
      Vector<String> thresholdValueXAxis = (Vector<String>)scriptDataTable.get("threshold_value_x_axis");
      if (thresholdValueXAxis == null || thresholdValueXAxis.elementAt(0) == null || thresholdValueXAxis.elementAt(0).length() == 0 || thresholdValueXAxis.elementAt(0).equals("null")){
        thresholdValueXAxisEditText.setText(null);
        sensorContext.getSensors().get(sensorHandle).getSensorStdFields(sensorReqHandle).setThresholdValueX(null);
      }else {
        thresholdValueXAxisEditText.setText(thresholdValueXAxis.elementAt(0));
        sensorContext.getSensors().get(sensorHandle).getSensorStdFields(sensorReqHandle).setFlushPeriod(thresholdValueXAxis.elementAt(0));
      }
    }
    USTALog.i("assignStaticDataFromScript  threshold_value_x_axis Done");
    thresholdValueYAxisEditText = (EditText) sensorRequestFragmentView.findViewById(R.id.threshold_value_axis_2);
    if(scriptDataTable != null && scriptDataTable.get("threshold_value_y_axis") != null) {
      Vector<String> thresholdValueYAxis = (Vector<String>)scriptDataTable.get("threshold_value_y_axis");
      if (thresholdValueYAxis == null || thresholdValueYAxis.elementAt(0) == null || thresholdValueYAxis.elementAt(0).length() == 0 || thresholdValueYAxis.elementAt(0).equals("null")){
        thresholdValueYAxisEditText.setText(null);
        sensorContext.getSensors().get(sensorHandle).getSensorStdFields(sensorReqHandle).setThresholdValueY(null);
      }else {
        thresholdValueYAxisEditText.setText(thresholdValueYAxis.elementAt(0));
        sensorContext.getSensors().get(sensorHandle).getSensorStdFields(sensorReqHandle).setThresholdValueY(thresholdValueYAxis.elementAt(0));
      }
    }
    USTALog.i("assignStaticDataFromScript  threshold_value_y_axis Done");
    thresholdValueZAxisEditText = (EditText) sensorRequestFragmentView.findViewById(R.id.threshold_value_axis_3);
    if(scriptDataTable != null && scriptDataTable.get("threshold_value_z_axis") != null) {
      Vector<String> thresholdValueZAxis = (Vector<String>)scriptDataTable.get("threshold_value_z_axis");
      if (thresholdValueZAxis == null || thresholdValueZAxis.elementAt(0) == null || thresholdValueZAxis.elementAt(0).length() == 0 || thresholdValueZAxis.elementAt(0).equals("null")){
        thresholdValueZAxisEditText.setText(null);
        sensorContext.getSensors().get(sensorHandle).getSensorStdFields(sensorReqHandle).setFlushPeriod(null);
      }else {
        thresholdValueZAxisEditText.setText(thresholdValueZAxis.elementAt(0));
        sensorContext.getSensors().get(sensorHandle).getSensorStdFields(sensorReqHandle).setFlushPeriod(thresholdValueZAxis.elementAt(0));
      }
    }
    USTALog.i("assignStaticDataFromScript  threshold_value_z_axis Done");
  }

  private void assignScriptData(Map scriptDataTable){
    USTALog.i("assignScriptData ");
    assignStaticDataFromScript(scriptDataTable);
    SensorPayloadFragment.getMyInstance().assignPayLoadDataFromScript(scriptDataTable);
  }

  private void setupPreDefinedSCriptSpinner(){

    preDefiniedScriptSpinner = (Spinner) sensorRequestFragmentView.findViewById(R.id.pre_defined_script_spinner);
    ArrayAdapter<String> preDefiniedScriptSpinnerAdapter = new ArrayAdapter<>(getActivity().getApplicationContext(), android.R.layout.simple_list_item_1, sensorContext.getScriptUtil().getScriptList(sensorContext.getSensors().get(sensorHandle).getSensorSUIDLow()));
    preDefiniedScriptSpinner.setAdapter(preDefiniedScriptSpinnerAdapter);
    USTALog.i("setupPreDefinedSCriptSpinner ");
    preDefiniedScriptSpinner.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
      @Override
      public void onItemSelected(AdapterView<?> parent, View view, int position, long id ) {
        Map scriptDataTable;
        if(preDefiniedScriptSpinner.getSelectedItemPosition() != 0 ){
          USTALog.i("setupPreDefinedSCriptSpinner position  " + position + " & " + preDefiniedScriptSpinner.getSelectedItemPosition());
          scriptDataTable = sensorContext.getScriptUtil().parseScriptFile(preDefiniedScriptSpinner.getSelectedItemPosition());
          assignScriptData(scriptDataTable);
        }
      }

      @Override
      public void onNothingSelected(AdapterView<?> parent) {

      }
    });

  }

  private void resetPreDefinedScriptsSpinner() {
      setupPreDefinedSCriptSpinner();
  }
  private void resetClientProcessorSpinner() {

    if (clientProcessorSpinner == null) {

      setupClientProcessorSpinner();
    } else {
      clientProcessorSpinner.setSelection(sensorContext.getSensors().get(sensorHandle).getSensorStdFields(sensorReqHandle).getClientProcessor());
    }
  }

  private void resetWakeupDeliverySpinner() {

    if (wakeupDeliverySpinner == null) {

      setupWakeupDeliverySpinner();
    } else {

      wakeupDeliverySpinner.setSelection(sensorContext.getSensors().get(sensorHandle).getSensorStdFields(sensorReqHandle).getWakeupDevilvery());
    }
  }

  private void resetResamplerSpinner() {
    if (resamplerTypeSpinner == null) {
      setUpResamplerTypeSpinner();
    } else {
      resamplerTypeSpinner.setSelection(sensorContext.getSensors().get(sensorHandle).getSensorStdFields(sensorReqHandle).getResamplerType());
    }
  }

  private void resetThresholdSpinner() {
    if (thresholdTypeSpinner == null) {
      setUpThresholdTypeSpinner();
    } else {
      thresholdTypeSpinner.setSelection(sensorContext.getSensors().get(sensorHandle).getSensorStdFields(sensorReqHandle).getThresholdType());
    }
  }
  private void resetClientConnectIDSpinner() {
  USTALog.i(" resetClientConnectIDSpinner start");
    if (clientConnectIdSpinner == null) {
    USTALog.i(" resetClientConnectIDSpinner Set up");
      setUpClientConnectIDSpinner();
    } else {
    USTALog.i(" resetClientConnectIDSpinner Set position " + sensorContext.getSensors().get(sensorHandle).getSensorStdFields(sensorReqHandle).getClientConnectID());
    int clientConnectID = sensorContext.getSensors().get(sensorHandle).getSensorStdFields(sensorReqHandle).getClientConnectID();
    int clientConnectIDPosition = 0;
    for(int loopIndex = 0; loopIndex < sensorContext.getClientConnectIdList().size();loopIndex++) {
      if(clientConnectID == Integer.parseInt(sensorContext.getClientConnectIdList().elementAt(loopIndex))) {
        clientConnectIDPosition = loopIndex;
        break;
      }
    }
    clientConnectIdSpinner.setSelection(clientConnectIDPosition);
    }
  }

  private void resetRequestMsgListSpinner() {

    if (sensorReqMsgSpinner == null) {

      setupRequestMsgListSpinner();
    } else {
      sensorContext.getSensors().get(sensorHandle).clearSensorReqMsgNamesWithMsgID();
      sensorContext.getSensors().get(sensorHandle).setSensorReqMsgsNamesWithMsgID();
      @SuppressWarnings("unchecked")
      Vector<String> sensorReqMsgNamesToAdapter = (Vector<String>) sensorContext.getSensors().get(sensorHandle).getSensorReqMsgsNamesWithMsgID().clone();

      int defaultSpinnerPosition = 0;
      for(int reqMsgIndex = 0 ; reqMsgIndex < sensorReqMsgNamesToAdapter.size() ; reqMsgIndex++){
        if(sensorReqMsgNamesToAdapter.get(reqMsgIndex).contains("513")) {
          defaultSpinnerPosition = reqMsgIndex;
          break;
        }
      }

      sensorReqMsgSpinnerAdapter.clear();
      sensorReqMsgSpinnerAdapter.addAll(sensorReqMsgNamesToAdapter);
      sensorReqMsgSpinnerAdapter.notifyDataSetChanged();
      sensorReqMsgSpinner.setSelection(defaultSpinnerPosition);
      sensorReqHandle = defaultSpinnerPosition;

      sensorContext.getSensors().get(sensorHandle).getSensorStdFields(sensorReqHandle).setBatchPeriod(null);
      batchPeriodEditText.setText(null);

      sensorContext.getSensors().get(sensorHandle).getSensorStdFields(sensorReqHandle).setFlushPeriod(null);
      flushPeriodEditText.setText(null);

      sensorContext.getSensors().get(sensorHandle).getSensorStdFields(sensorReqHandle).setFlushOnly(false);
      flushOnlyCB.setChecked(false);

      sensorContext.getSensors().get(sensorHandle).getSensorStdFields(sensorReqHandle).setMaxBatch(false);
      maxBatchCB.setChecked(false);

      sensorContext.getSensors().get(sensorHandle).getSensorStdFields(sensorReqHandle).setPassive(false);
      isPassiveCB.setChecked(false);

      sensorContext.getSensors().get(sensorHandle).getSensorStdFields(sensorReqHandle).setResampled(false);
      resamplerConfigCB.setChecked(false);

      sensorContext.getSensors().get(sensorHandle).getSensorStdFields(sensorReqHandle).setResamplerFilter(false);
      resamplerFilterCB.setChecked(false);

      sensorContext.getSensors().get(sensorHandle).getSensorStdFields(sensorReqHandle).setThresholded(false);
      thresholdConfigCB.setChecked(false);

      sensorContext.getSensors().get(sensorHandle).getSensorStdFields(sensorReqHandle).setThresholdValueX(null);
      thresholdValueXAxisEditText.setText(null);

      sensorContext.getSensors().get(sensorHandle).getSensorStdFields(sensorReqHandle).setThresholdValueY(null);
      thresholdValueYAxisEditText.setText(null);

      sensorContext.getSensors().get(sensorHandle).getSensorStdFields(sensorReqHandle).setThresholdValueZ(null);
      thresholdValueZAxisEditText.setText(null);
      setupSensorPayloadFragment();
    }

    resetClientProcessorSpinner();
    resetWakeupDeliverySpinner();
    resetResamplerSpinner();
    resetThresholdSpinner();
    resetClientConnectIDSpinner();
  }

  private void setupRequestMsgListSpinner() {

    sensorReqMsgSpinner = (Spinner) sensorRequestFragmentView.findViewById(R.id.req_msg_list_spinner);

    sensorContext.getSensors().get(sensorHandle).clearSensorReqMsgNamesWithMsgID();
    sensorContext.getSensors().get(sensorHandle).setSensorReqMsgsNamesWithMsgID();
    Vector<String> sensorReqMsgNames = (Vector<String>) sensorContext.getSensors().get(sensorHandle).getSensorReqMsgsNamesWithMsgID();

    if ((null == sensorReqMsgNames) || sensorReqMsgNames.size() == 0) {
      return;
    }
    int defaultSpinnerPosition = 0;
    for(int reqMsgIndex = 0 ; reqMsgIndex < sensorReqMsgNames.size() ; reqMsgIndex++){
      if(sensorReqMsgNames.get(reqMsgIndex).contains("513")) {
        defaultSpinnerPosition = reqMsgIndex;
        break;
      }
    }

    Vector<String> sensorReqMsgNamesToAdapter = (Vector<String>) sensorReqMsgNames.clone();
    sensorReqMsgSpinnerAdapter = new ArrayAdapter<>(getActivity().getApplicationContext(), android.R.layout.simple_list_item_1, sensorReqMsgNamesToAdapter);
    sensorReqMsgSpinner.setAdapter(sensorReqMsgSpinnerAdapter);
    sensorReqMsgSpinner.setSelection(defaultSpinnerPosition);
    sensorReqHandle = defaultSpinnerPosition;

    sensorReqMsgSpinner.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
      @Override
      public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {

        sensorReqHandle = sensorReqMsgSpinner.getSelectedItemPosition();
        setupSensorPayloadFragment();

        resetClientProcessorSpinner();
        resetWakeupDeliverySpinner();
        batchPeriodEditText.setText(null /*sensorContext.getSensors().get(sensorHandle).getSensorStdFields(sensorReqHandle).getBatchPeriod()*/);
        flushPeriodEditText.setText(null /*sensorContext.getSensors().get(sensorHandle).getSensorStdFields(sensorReqHandle).getFlushPeriod()*/);
        flushOnlyCB.setChecked(false /*sensorContext.getSensors().get(sensorHandle).getSensorStdFields(sensorReqHandle).isFlushOnly()*/);
        maxBatchCB.setChecked(false /*sensorContext.getSensors().get(sensorHandle).getSensorStdFields(sensorReqHandle).isMaxBatch()*/);
        isPassiveCB.setChecked(false);
      }

      @Override
      public void onNothingSelected(AdapterView<?> parent) {

      }
    });
  }

  private void setupClientProcessorSpinner() {

    clientProcessorSpinner = (Spinner) sensorRequestFragmentView.findViewById(R.id.client_processor_spinner);
    ArrayAdapter<String> clientProcessorSpinnerAdapter = new ArrayAdapter<>(getActivity().getApplicationContext(), android.R.layout.simple_list_item_1, sensorContext.getClientProcessors());
    clientProcessorSpinner.setAdapter(clientProcessorSpinnerAdapter);

    clientProcessorSpinner.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
      @Override
      public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {

        sensorContext.getSensors().get(sensorHandle).getSensorStdFields(sensorReqHandle).setClientProcessor(clientProcessorSpinner.getSelectedItemPosition());
      }

      @Override
      public void onNothingSelected(AdapterView<?> parent) {

      }
    });
  }

  private void setupWakeupDeliverySpinner() {

    wakeupDeliverySpinner = (Spinner) sensorRequestFragmentView.findViewById(R.id.wakeup_spinner);
    ArrayAdapter<String> wakeupDeliverySpinnerAdapter = new ArrayAdapter<>(getActivity().getApplicationContext(), android.R.layout.simple_list_item_1, sensorContext.getWakeupDelivery());
    wakeupDeliverySpinner.setAdapter(wakeupDeliverySpinnerAdapter);

    wakeupDeliverySpinner.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
      @Override
      public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {

        sensorContext.getSensors().get(sensorHandle).getSensorStdFields(sensorReqHandle).setWakeupDevilvery(wakeupDeliverySpinner.getSelectedItemPosition());
      }

      @Override
      public void onNothingSelected(AdapterView<?> parent) {

      }
    });
  }

  private void setupBatchPeriodEditText() {

    batchPeriodEditText = (EditText) sensorRequestFragmentView.findViewById(R.id.batch_period_input_text);
    batchPeriodEditText.setOnFocusChangeListener(new View.OnFocusChangeListener() {
      @Override
      public void onFocusChange(View v, boolean hasFocus) {

        if (v.getId() == R.id.batch_period_input_text && !hasFocus) {
          InputMethodManager imm = (InputMethodManager) getActivity().getApplicationContext().getSystemService(Context.INPUT_METHOD_SERVICE);
          imm.hideSoftInputFromWindow(v.getWindowToken(), 0);
        }
      }
    });

    TextWatcher batchPeriodTextWatcher = new TextWatcher() {
      @Override
      public void beforeTextChanged(CharSequence s, int start, int count, int after) {

      }

      @Override
      public void onTextChanged(CharSequence s, int start, int before, int count) {

        if (s.length() != 0) {
          sensorContext.getSensors().get(sensorHandle).getSensorStdFields(sensorReqHandle).setBatchPeriod(s.toString());
        } else
          sensorContext.getSensors().get(sensorHandle).getSensorStdFields(sensorReqHandle).setBatchPeriod(null);
      }

      @Override
      public void afterTextChanged(Editable s) {

      }
    };

    batchPeriodEditText.addTextChangedListener(batchPeriodTextWatcher);
  }

  private void setupFlushOnlyCheckBox () {
    flushOnlyCB    =   (CheckBox) sensorRequestFragmentView.findViewById(R.id.flush_only_checkbox);
    flushOnlyCB.setOnClickListener(new View.OnClickListener()
    {
      public void onClick(View v) {
        if (((CheckBox)v).isChecked()) {
          sensorContext.getSensors().get(sensorHandle).getSensorStdFields(sensorReqHandle).setFlushOnly(true);
        } else {
          sensorContext.getSensors().get(sensorHandle).getSensorStdFields(sensorReqHandle).setFlushOnly(false);
        }
      }
    });
  }

  private void setupMaxBatchCheckBox () {
    maxBatchCB    =   (CheckBox) sensorRequestFragmentView.findViewById(R.id.max_batch_checkbox);
    maxBatchCB.setOnClickListener(new View.OnClickListener()
    {
      public void onClick(View v) {
        if (((CheckBox)v).isChecked()) {
          sensorContext.getSensors().get(sensorHandle).getSensorStdFields(sensorReqHandle).setMaxBatch(true);
        } else {
          sensorContext.getSensors().get(sensorHandle).getSensorStdFields(sensorReqHandle).setMaxBatch(false);
        }
      }
    });
  }

  private void setupIsPassiveCheckBox () {
    isPassiveCB    =   (CheckBox) sensorRequestFragmentView.findViewById(R.id.is_passive_checkbox);
    isPassiveCB.setOnClickListener(new View.OnClickListener()
    {
      public void onClick(View v) {
        if (((CheckBox)v).isChecked()) {
          sensorContext.getSensors().get(sensorHandle).getSensorStdFields(sensorReqHandle).setPassive(true);
        } else {
          sensorContext.getSensors().get(sensorHandle).getSensorStdFields(sensorReqHandle).setPassive(false);
        }
      }
    });
  }

  private void setUpResamplerTypeSpinner() {

    resamplerTypeSpinner = (Spinner) sensorRequestFragmentView.findViewById(R.id.resampler_type_spinner_id);

    ArrayAdapter<String> resamplerTypeSpinnerAdapter = new ArrayAdapter<>(getActivity().getApplicationContext(),android.R.layout.simple_list_item_1, sensorContext.getResamplerTypes());
    resamplerTypeSpinnerAdapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
    resamplerTypeSpinner.setAdapter(resamplerTypeSpinnerAdapter);

    resamplerTypeSpinner.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
      @Override
      public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
        sensorContext.getSensors().get(sensorHandle).getSensorStdFields(sensorReqHandle).setResamplerType(resamplerTypeSpinner.getSelectedItemPosition());
      }

      @Override
      public void onNothingSelected(AdapterView<?> parent) {

      }
    });
  }

  private void setUpResamplerFilterCheckBox() {
    resamplerFilterCB    =   (CheckBox) sensorRequestFragmentView.findViewById(R.id.resampler_filter_checkBox);
    resamplerFilterCB.setOnClickListener(new View.OnClickListener()
    {
      public void onClick(View v) {
        if (((CheckBox)v).isChecked()) {
          sensorContext.getSensors().get(sensorHandle).getSensorStdFields(sensorReqHandle).setResamplerFilter(true);
        } else {
          sensorContext.getSensors().get(sensorHandle).getSensorStdFields(sensorReqHandle).setResamplerFilter(false);
        }
      }
    });
  }

  private void setUpResamplerConfigCheckBox() {
    resamplerConfigCB    =   (CheckBox) sensorRequestFragmentView.findViewById(R.id.is_resample_checkbox);
    resamplerConfigCB.setOnClickListener(new View.OnClickListener()
    {
      public void onClick(View v) {
        if (((CheckBox)v).isChecked()) {
          sensorContext.getSensors().get(sensorHandle).getSensorStdFields(sensorReqHandle).setResampled(true);
        } else {
          sensorContext.getSensors().get(sensorHandle).getSensorStdFields(sensorReqHandle).setResampled(false);
        }
      }
    });
  }

  private void setUpThresholdConfigCheckBox() {
    thresholdConfigCB    =   (CheckBox) sensorRequestFragmentView.findViewById(R.id.is_threshold_checkBox);
    thresholdConfigCB.setOnClickListener(new View.OnClickListener()
    {
      public void onClick(View v) {
        if (((CheckBox)v).isChecked()) {
          sensorContext.getSensors().get(sensorHandle).getSensorStdFields(sensorReqHandle).setThresholded(true);
        } else {
          sensorContext.getSensors().get(sensorHandle).getSensorStdFields(sensorReqHandle).setThresholded(false);
        }
      }
    });
  }

  private void setUpThresholdTypeSpinner() {

    thresholdTypeSpinner = (Spinner) sensorRequestFragmentView.findViewById(R.id.threshold_type_spinner_id);
    ArrayAdapter<String> thresholdTypeSpinnerAdapter = new ArrayAdapter<>(getActivity().getApplicationContext(), android.R.layout.simple_list_item_1, sensorContext.getThresholdTypes());
    thresholdTypeSpinnerAdapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
    thresholdTypeSpinner.setAdapter(thresholdTypeSpinnerAdapter);

    thresholdTypeSpinner.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
      @Override
      public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
         sensorContext.getSensors().get(sensorHandle).getSensorStdFields(sensorReqHandle).setThresholdType(thresholdTypeSpinner.getSelectedItemPosition());
      }

      @Override
      public void onNothingSelected(AdapterView<?> parent) {

      }
    });
  }
  private void setUpThresholdValueXAxisEditeText() {

    thresholdValueXAxisEditText = (EditText) sensorRequestFragmentView.findViewById(R.id.threshold_value_axis_1);
    thresholdValueXAxisEditText.setOnFocusChangeListener(new View.OnFocusChangeListener() {
      @Override
      public void onFocusChange(View v, boolean hasFocus) {

        if (v.getId() == R.id.threshold_value_axis_1 && !hasFocus) {
          InputMethodManager imm = (InputMethodManager) getActivity().getApplicationContext().getSystemService(Context.INPUT_METHOD_SERVICE);
          imm.hideSoftInputFromWindow(v.getWindowToken(), 0);
        }
      }
    });

    TextWatcher flushPeriodTextWatcher = new TextWatcher() {
      @Override
      public void beforeTextChanged(CharSequence s, int start, int count, int after) {

      }

      @Override
      public void onTextChanged(CharSequence s, int start, int before, int count) {

        if (s.length() != 0) {
          sensorContext.getSensors().get(sensorHandle).getSensorStdFields(sensorReqHandle).setThresholdValueX(s.toString());
        } else
          sensorContext.getSensors().get(sensorHandle).getSensorStdFields(sensorReqHandle).setThresholdValueX(null);
      }

      @Override
      public void afterTextChanged(Editable s) {

      }
    };

    thresholdValueXAxisEditText.addTextChangedListener(flushPeriodTextWatcher);
  }

  private void setUpThresholdValueYAxisEditeText() {

    thresholdValueYAxisEditText = (EditText) sensorRequestFragmentView.findViewById(R.id.threshold_value_axis_2);
    thresholdValueYAxisEditText.setOnFocusChangeListener(new View.OnFocusChangeListener() {
      @Override
      public void onFocusChange(View v, boolean hasFocus) {

        if (v.getId() == R.id.threshold_value_axis_2 && !hasFocus) {
          InputMethodManager imm = (InputMethodManager) getActivity().getApplicationContext().getSystemService(Context.INPUT_METHOD_SERVICE);
          imm.hideSoftInputFromWindow(v.getWindowToken(), 0);
        }
      }
    });

    TextWatcher flushPeriodTextWatcher = new TextWatcher() {
      @Override
      public void beforeTextChanged(CharSequence s, int start, int count, int after) {

      }

      @Override
      public void onTextChanged(CharSequence s, int start, int before, int count) {

        if (s.length() != 0) {
          sensorContext.getSensors().get(sensorHandle).getSensorStdFields(sensorReqHandle).setThresholdValueY(s.toString());
        } else
          sensorContext.getSensors().get(sensorHandle).getSensorStdFields(sensorReqHandle).setThresholdValueY(null);
      }

      @Override
      public void afterTextChanged(Editable s) {

      }
    };

    thresholdValueYAxisEditText.addTextChangedListener(flushPeriodTextWatcher);
  }

  private void setUpThresholdValueZAxisEditeText() {

    thresholdValueZAxisEditText = (EditText) sensorRequestFragmentView.findViewById(R.id.threshold_value_axis_3);
    thresholdValueZAxisEditText.setOnFocusChangeListener(new View.OnFocusChangeListener() {
      @Override
      public void onFocusChange(View v, boolean hasFocus) {

        if (v.getId() == R.id.threshold_value_axis_3 && !hasFocus) {
          InputMethodManager imm = (InputMethodManager) getActivity().getApplicationContext().getSystemService(Context.INPUT_METHOD_SERVICE);
          imm.hideSoftInputFromWindow(v.getWindowToken(), 0);
        }
      }
    });

    TextWatcher flushPeriodTextWatcher = new TextWatcher() {
      @Override
      public void beforeTextChanged(CharSequence s, int start, int count, int after) {

      }

      @Override
      public void onTextChanged(CharSequence s, int start, int before, int count) {

        if (s.length() != 0) {
          sensorContext.getSensors().get(sensorHandle).getSensorStdFields(sensorReqHandle).setThresholdValueZ(s.toString());
        } else
          sensorContext.getSensors().get(sensorHandle).getSensorStdFields(sensorReqHandle).setThresholdValueZ(null);
      }

      @Override
      public void afterTextChanged(Editable s) {

      }
    };

    thresholdValueZAxisEditText.addTextChangedListener(flushPeriodTextWatcher);
  }
  private void setUpClientConnectIDSpinner() {

    ArrayAdapter<String> clientConnectIdSpinnerAdapter = new ArrayAdapter<>(getActivity().getApplicationContext(), android.R.layout.simple_list_item_1, sensorContext.getClientConnectIdList());
    clientConnectIdSpinner = (Spinner) sensorRequestFragmentView.findViewById(R.id.client_connect_spinner_id);
    clientConnectIdSpinner.setAdapter(clientConnectIdSpinnerAdapter);
    clientConnectIdSpinner.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
      @Override
      public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
        USTALog.i("Seletected position " + clientConnectIdSpinner.getSelectedItemPosition());
        USTALog.i("Selected ClientConnectID " + Integer.parseInt(sensorContext.getClientConnectIdList().elementAt(clientConnectIdSpinner.getSelectedItemPosition())));
        sensorContext.getSensors().get(sensorHandle).getSensorStdFields(sensorReqHandle).setClientConnectID(Integer.parseInt(sensorContext.getClientConnectIdList().elementAt(clientConnectIdSpinner.getSelectedItemPosition())));
      }
      @Override
      public void onNothingSelected(AdapterView<?> parent) {
      }
    });

  }

  private void setupFlushPeriodEditText() {

    flushPeriodEditText = (EditText) sensorRequestFragmentView.findViewById(R.id.flush_period_input_text);
    flushPeriodEditText.setOnFocusChangeListener(new View.OnFocusChangeListener() {
      @Override
      public void onFocusChange(View v, boolean hasFocus) {

        if (v.getId() == R.id.flush_period_input_text && !hasFocus) {
          InputMethodManager imm = (InputMethodManager) getActivity().getApplicationContext().getSystemService(Context.INPUT_METHOD_SERVICE);
          imm.hideSoftInputFromWindow(v.getWindowToken(), 0);
        }
      }
    });

    TextWatcher flushPeriodTextWatcher = new TextWatcher() {
      @Override
      public void beforeTextChanged(CharSequence s, int start, int count, int after) {

      }

      @Override
      public void onTextChanged(CharSequence s, int start, int before, int count) {

        if (s.length() != 0) {
          sensorContext.getSensors().get(sensorHandle).getSensorStdFields(sensorReqHandle).setFlushPeriod(s.toString());
        } else
          sensorContext.getSensors().get(sensorHandle).getSensorStdFields(sensorReqHandle).setFlushPeriod(null);
      }

      @Override
      public void afterTextChanged(Editable s) {

      }
    };

    flushPeriodEditText.addTextChangedListener(flushPeriodTextWatcher);
  }

  private void setupSensorPayloadFragment() {

    FragmentManager fm = getActivity().getFragmentManager();
    Fragment sensorPayloadFragment = SensorPayloadFragment.CreateSensorPayloadFragmentInstance(sensorHandle, sensorReqHandle, sensorContext, false);

    if (fm.findFragmentById(R.id.sensor_payload_fragment) != null) {

      fm.beginTransaction().replace(R.id.sensor_payload_fragment, sensorPayloadFragment).commit();
    } else {

      fm.beginTransaction().add(R.id.sensor_payload_fragment, sensorPayloadFragment).commit();
    }
  }

  private void setupSensorEvenPayloadFragment() {

    FragmentManager fm = getActivity().getFragmentManager();
    Fragment sensorEventPayloadFragment = SensorEventDisplayFragment.CreateDisplaySamplesInstance();
    fm.beginTransaction().add(R.id.sensor_event_payload_display_id, sensorEventPayloadFragment).commit();
  }

  public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceData) {

    if (sensorContext == null) {

      USTALog.e("sensorContext is null so Killing the activity ");

      SensorContext.throwErrorDialog("No Sensors Found! Please check if proto files are loaded.", getContext());

      return null;
    }

    sensorRequestFragmentView = inflater.inflate(R.layout.fragment_sensor_request, container, false);
    setupSensorListSpinner();
    setupBatchPeriodEditText();
    setupFlushPeriodEditText();
    setupFlushOnlyCheckBox();
    setupMaxBatchCheckBox();
    setupIsPassiveCheckBox();
    setUpResamplerConfigCheckBox();
    setUpResamplerTypeSpinner();
    setUpResamplerFilterCheckBox();
    setUpThresholdConfigCheckBox();
    setUpThresholdTypeSpinner();
    setUpThresholdValueXAxisEditeText();
    setUpThresholdValueYAxisEditeText();
    setUpThresholdValueZAxisEditeText();
    setUpClientConnectIDSpinner();

    setupSensorEvenPayloadFragment();
    return sensorRequestFragmentView;
  }

}
