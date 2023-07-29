import time
import datetime;
import paho.mqtt.client as mqtt
import ssl
import json
import _thread

def on_connect(client, userdata, flags, rc):
    print("Connected to AWS IoT")

client = mqtt.Client()
client.on_connect = on_connect
client.tls_set(ca_certs='/home/sanslab/Desktop/tf_pi/rx_service/src/AmazonRootCA1.pem', certfile='/home/sanslab/Desktop/tf_pi/rx_service/src/certificate.pem.crt', keyfile='/home/sanslab/Desktop/tf_pi/rx_service/src/private.pem.key', tls_version=ssl.PROTOCOL_SSLv23)
client.tls_insecure_set(True)
client.connect("an9hbpmwya55-ats.iot.ap-northeast-2.amazonaws.com", 8883, 60)

def publish_normal(data_normal):
    client.loop_start()
    data = [float(value) for value in data_normal.split(',')]

    #print ("Temperature = %.1f" % data[0])
    #print ("Salt = %.1f" % data[1])
    #print ("PH = %.1f" % data[2])
    #print ("NH3 = %.3f" % data[3])
    #print ("H2S = %.3f" % data[4])
    #print ("TSS = %.1f" % data[5])
    #print ("DO = %.1f" % data[6])
    #print ("COD = %.1f" % data[7])

    timestamp = datetime.datetime.now().strftime("%m/%d/%Y, %H:%M:%S")
    client.publish("data_normal", payload=json.dumps({"timestamp": timestamp, "data": "Data_connect"}), qos=0, retain=False)
    time.sleep(5)
    
    client.publish("data/normal", payload=json.dumps({"timestamp": timestamp, "Temperature": data[0], "Salt": data[1], "PH": data[2], "NH3": data[3], "H2S": data[4], "TSS": data[5], "DO": data[6], "COD": data[7]}), qos=0, retain=False)
    client.loop_stop()

def publish_predict(data_predict):
    client.loop_start()
    data = [float(value) for value in data_predict.split(',')]

    #print ("Temperature = %.1f" % data[0])
    #print ("Salt = %.1f" % data[1])
    #print ("PH = %.1f" % data[2])
    #print ("NH3 = %.3f" % data[3])
    #print ("H2S = %.3f" % data[4])
    #print ("TSS = %.1f" % data[5])
    #print ("DO = %.1f" % data[6])
    #print ("COD = %.1f" % data[7])

    timestamp = datetime.datetime.now().strftime("%m/%d/%Y, %H:%M:%S")
    client.publish("data/predict", payload=json.dumps({"timestamp": timestamp, "data": "Data_connect"}), qos=0, retain=False)
    time.sleep(5)
    
    client.publish("data/predict", payload=json.dumps({"timestamp": timestamp, "Temperature": data[0], "Salt": data[1], "PH": data[2], "NH3": data[3], "H2S": data[4], "TSS": data[5], "DO": data[6], "COD": data[7]}), qos=0, retain=False)
    client.loop_stop()

def publishData(data_normal, data_predict):
    publish_normal(data_normal)
    publish_predict(data_predict)
    return 0