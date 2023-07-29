from tflite_runtime.interpreter import Interpreter
import numpy as np
import pandas
from sklearn.preprocessing import MinMaxScaler
import time

scaler = MinMaxScaler()
data_temp = pandas.read_csv('/home/sanslab/Desktop/tf_pi/rx_service/src/data_origin/Temp.csv', usecols=['Temperature Number'])
data_salt = pandas.read_csv('/home/sanslab/Desktop/tf_pi/rx_service/src/data_origin/Salt.csv', usecols=['Salt'])
data_ph = pandas.read_csv('/home/sanslab/Desktop/tf_pi/rx_service/src/data_origin/PH.csv', usecols=['PH'])
data_nh3 = pandas.read_csv('/home/sanslab/Desktop/tf_pi/rx_service/src/data_origin/NH3.csv', usecols=['NH3'])
data_h2s = pandas.read_csv('/home/sanslab/Desktop/tf_pi/rx_service/src/data_origin/H2S.csv', usecols=['H2S'])
data_tss = pandas.read_csv('/home/sanslab/Desktop/tf_pi/rx_service/src/data_origin/TSS.csv', usecols=['TSS'])
data_do = pandas.read_csv('/home/sanslab/Desktop/tf_pi/rx_service/src/data_origin/DO.csv', usecols=['DO'])
data_cod = pandas.read_csv('/home/sanslab/Desktop/tf_pi/rx_service/src/data_origin/COD.csv', usecols=['COD'])

def predict_temp(input_temp):
    inputs = data_temp[0:59].values.tolist()
    inputs.append([input_temp])
    inputs_scale = scaler.fit_transform(inputs)
    inputs_scale = inputs_scale.reshape(1,60,1)
    # Load TFLite model and allocate tensors.
    
    interpreter = Interpreter(model_path="/home/sanslab/Desktop/tf_pi/rx_service/src/LSTM_model/LSTM_model_2_temp.tflite")
    interpreter.allocate_tensors()
    # Get input and output tensors.
    
    input_details = interpreter.get_input_details()
    output_details = interpreter.get_output_details()
    inputs_scale_float32 = np.float32(inputs_scale)
    interpreter.set_tensor(input_details[0]['index'], inputs_scale_float32)
    interpreter.invoke()
    
    # The function `get_tensor()` returns a copy of the tensor data.
    # Use `tensor()` in order to get a pointer to the tensor.
    output_data = interpreter.get_tensor(output_details[0]['index'])
    y_pred_RNN_model = scaler.inverse_transform(output_data)
    
    return y_pred_RNN_model.item()

def predict_salt(input_salt):
    inputs = data_salt[0:59].values.tolist()
    inputs.append([input_salt])
    inputs_scale = scaler.fit_transform(inputs)
    inputs_scale = inputs_scale.reshape(1,60,1)
    
    # Load TFLite model and allocate tensors.
    interpreter = Interpreter(model_path="/home/sanslab/Desktop/tf_pi/rx_service/src/LSTM_model/LSTM_model_4_salt.tflite")
    interpreter.allocate_tensors()
    
    # Get input and output tensors.
    input_details = interpreter.get_input_details()
    output_details = interpreter.get_output_details()
    inputs_scale_float32 = np.float32(inputs_scale)
    interpreter.set_tensor(input_details[0]['index'], inputs_scale_float32)
    interpreter.invoke()

    # The function `get_tensor()` returns a copy of the tensor data.
    # Use `tensor()` in order to get a pointer to the tensor.
    output_data = interpreter.get_tensor(output_details[0]['index'])
    y_pred_RNN_model = scaler.inverse_transform(output_data)
    
    return y_pred_RNN_model.item()

def predict_ph(input_ph):
    inputs = data_ph[0:59].values.tolist()
    inputs.append([input_ph])
    inputs_scale = scaler.fit_transform(inputs)
    inputs_scale = inputs_scale.reshape(1,60,1)
    
    # Load TFLite model and allocate tensors.
    interpreter = Interpreter(model_path="/home/sanslab/Desktop/tf_pi/rx_service/src/LSTM_model/LSTM_model_2_PH.tflite")
    interpreter.allocate_tensors()
    
    # Get input and output tensors.
    input_details = interpreter.get_input_details()
    output_details = interpreter.get_output_details()
    inputs_scale_float32 = np.float32(inputs_scale)
    interpreter.set_tensor(input_details[0]['index'], inputs_scale_float32)
    interpreter.invoke()

    # The function `get_tensor()` returns a copy of the tensor data.
    # Use `tensor()` in order to get a pointer to the tensor.
    output_data = interpreter.get_tensor(output_details[0]['index'])
    y_pred_RNN_model = scaler.inverse_transform(output_data)
    
    return y_pred_RNN_model.item()

def predict_nh3(input_nh3):
    inputs = data_nh3[0:59].values.tolist()
    inputs.append([input_nh3])
    inputs_scale = scaler.fit_transform(inputs)
    inputs_scale = inputs_scale.reshape(1,60,1)
    
    # Load TFLite model and allocate tensors.
    interpreter = Interpreter(model_path="/home/sanslab/Desktop/tf_pi/rx_service/src/LSTM_model/LSTM_model_5_NH3.tflite")
    interpreter.allocate_tensors()
    
    # Get input and output tensors.
    input_details = interpreter.get_input_details()
    output_details = interpreter.get_output_details()
    inputs_scale_float32 = np.float32(inputs_scale)
    interpreter.set_tensor(input_details[0]['index'], inputs_scale_float32)
    interpreter.invoke()

    # The function `get_tensor()` returns a copy of the tensor data.
    # Use `tensor()` in order to get a pointer to the tensor.
    output_data = interpreter.get_tensor(output_details[0]['index'])
    y_pred_RNN_model = scaler.inverse_transform(output_data)
    
    return y_pred_RNN_model.item()

def predict_h2s(input_h2s):
    inputs = data_h2s[0:59].values.tolist()
    inputs.append([input_h2s])
    inputs_scale = scaler.fit_transform(inputs)
    inputs_scale = inputs_scale.reshape(1,60,1)
    
    # Load TFLite model and allocate tensors.
    interpreter = Interpreter(model_path="/home/sanslab/Desktop/tf_pi/rx_service/src/LSTM_model/LSTM_model_5_H2S.tflite")
    interpreter.allocate_tensors()
    
    # Get input and output tensors.
    input_details = interpreter.get_input_details()
    output_details = interpreter.get_output_details()
    inputs_scale_float32 = np.float32(inputs_scale)
    interpreter.set_tensor(input_details[0]['index'], inputs_scale_float32)
    interpreter.invoke()

    # The function `get_tensor()` returns a copy of the tensor data.
    # Use `tensor()` in order to get a pointer to the tensor.
    output_data = interpreter.get_tensor(output_details[0]['index'])
    y_pred_RNN_model = scaler.inverse_transform(output_data)
    
    return y_pred_RNN_model.item()

def predict_tss(input_tss):
    inputs = data_tss[0:59].values.tolist()
    inputs.append([input_tss])
    inputs_scale = scaler.fit_transform(inputs)
    inputs_scale = inputs_scale.reshape(1,60,1)
    
    # Load TFLite model and allocate tensors.
    interpreter = Interpreter(model_path="/home/sanslab/Desktop/tf_pi/rx_service/src/LSTM_model/LSTM_model_4_TSS.tflite")
    interpreter.allocate_tensors()
    
    # Get input and output tensors.
    input_details = interpreter.get_input_details()
    output_details = interpreter.get_output_details()
    inputs_scale_float32 = np.float32(inputs_scale)
    interpreter.set_tensor(input_details[0]['index'], inputs_scale_float32)
    interpreter.invoke()

    # The function `get_tensor()` returns a copy of the tensor data.
    # Use `tensor()` in order to get a pointer to the tensor.
    output_data = interpreter.get_tensor(output_details[0]['index'])
    y_pred_RNN_model = scaler.inverse_transform(output_data)
    
    return y_pred_RNN_model.item()

def predict_do(input_do):
    inputs = data_do[0:59].values.tolist()
    inputs.append([input_do])
    inputs_scale = scaler.fit_transform(inputs)
    inputs_scale = inputs_scale.reshape(1,60,1)
    
    # Load TFLite model and allocate tensors.
    interpreter = Interpreter(model_path="/home/sanslab/Desktop/tf_pi/rx_service/src/LSTM_model/LSTM_model_2_DO.tflite")
    interpreter.allocate_tensors()
    
    # Get input and output tensors.
    input_details = interpreter.get_input_details()
    output_details = interpreter.get_output_details()
    inputs_scale_float32 = np.float32(inputs_scale)
    interpreter.set_tensor(input_details[0]['index'], inputs_scale_float32)
    interpreter.invoke()

    # The function `get_tensor()` returns a copy of the tensor data.
    # Use `tensor()` in order to get a pointer to the tensor.
    output_data = interpreter.get_tensor(output_details[0]['index'])
    y_pred_RNN_model = scaler.inverse_transform(output_data)
    
    return y_pred_RNN_model.item()

def predict_cod(input_cod):
    inputs = data_cod[0:59].values.tolist()
    inputs.append([input_cod])
    inputs_scale = scaler.fit_transform(inputs)
    inputs_scale = inputs_scale.reshape(1,60,1)
    
    # Load TFLite model and allocate tensors.
    interpreter = Interpreter(model_path="/home/sanslab/Desktop/tf_pi/rx_service/src/LSTM_model/LSTM_model_2_COD.tflite")
    interpreter.allocate_tensors()
    
    # Get input and output tensors.
    input_details = interpreter.get_input_details()
    output_details = interpreter.get_output_details()
    inputs_scale_float32 = np.float32(inputs_scale)
    interpreter.set_tensor(input_details[0]['index'], inputs_scale_float32)
    interpreter.invoke()

    # The function `get_tensor()` returns a copy of the tensor data.
    # Use `tensor()` in order to get a pointer to the tensor.
    output_data = interpreter.get_tensor(output_details[0]['index'])
    y_pred_RNN_model = scaler.inverse_transform(output_data)
    
    return y_pred_RNN_model.item()
    
def predict_list_params(input_temp,input_salt,input_ph,input_nh3,input_h2s,input_tss,input_do,input_cod):
    temp_predict = predict_temp(input_temp)
    salt_predict = predict_salt(input_salt)
    ph_predict = predict_ph(input_ph)
    nh3_predict = predict_nh3(input_nh3)
    h2s_predict = predict_h2s(input_h2s)
    tss_predict = predict_tss(input_tss)
    do_predict = predict_do(input_do)
    cod_predict = predict_cod(input_cod)
    
    temp_predict_str = f'{temp_predict:.1f}'
    salt_predict_str = f'{salt_predict:.1f}'
    ph_predict_str = f'{ph_predict:.1f}'
    nh3_predict_str = f'{nh3_predict:.3f}'
    h2s_predict_str = f'{h2s_predict:.3f}'
    tss_predict_str = f'{tss_predict:.3f}'
    do_predict_str = f'{do_predict:.1f}'
    cod_predict_str = f'{cod_predict:.1f}'
    
    predict_list_str = temp_predict_str + ',' + salt_predict_str + ',' + ph_predict_str + ',' + nh3_predict_str + ',' + h2s_predict_str + ',' + tss_predict_str + ',' + do_predict_str + ',' + cod_predict_str
    
    return predict_list_str