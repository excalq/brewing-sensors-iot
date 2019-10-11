# MQTT to Elasticsearch: 2019-10-11: Arthur Kepler (@excalq)

# Based on code by Matthew Field http://www.smart-factory.net
# distributed under GNU public license https://www.gnu.org/licenses/gpl.txt

# this program requires the script to be run on the same server as you
# have elasticsearch running

import paho.mqtt.client as mqtt
from datetime import date, datetime
from elasticsearch import Elasticsearch
import json
import traceback

#### CHANGE THIS FOR YOUR SETUP #####
mqttServer = "127.0.0.1"
mqttPort = 1883
mqttUser = "MYUSER"
mqttPass = "MYPASS"
#############################

#channelSubs="$SYS/#"
#use below as alternative to subscribe to all channels
channelSubs="#"

# by default we connect to elasticSearch on localhost:9200
es = Elasticsearch()

# The callback for when the client receives a CONNACK response from the server.
def on_connect(client, userdata, flags, rc):
    print("Connected with result code "+str(rc))
    # Subscribing in on_connect() means that if we lose the connection and
    # reconnect then subscriptions will be renewed.
    client.subscribe(channelSubs)

# The callback for when a PUBLISH message is received from the server.
def on_message(client, userdata, msg):

    # Handle value types of float, JSON, or string.
    # JSONic values will be inserted in ES with keys as fields
    try:
        string = msg.payload.decode('utf-8')
        today = date.today().isoformat()

        try:
            # Attempt to cast as a Float, which we put in an aggregatable field
            print("  " + msg.topic + " (float) " + str(float(msg.payload)))
            es.index(index="oort_sensors-{}".format(today), body={"topic": msg.topic, "valueFloat": float(msg.payload), "timestamp": datetime.utcnow()})

        except:
            # If valid JSON, use each key as a fieldname
            try:
                print("  " +  msg.topic+" "+str(msg.payload))
                json_value = json.loads(msg.payload.decode('utf-8'))
            except ValueError as e:
                # Plain String value
                print("  Proccessing " + string + " as a String")
                es.index(index="oort_sensors-{}".format(today), body={"topic": msg.topic, "value": string, "timestamp": datetime.utcnow()}) 
            except Exception as e:
                print("Warning: Exception thrown: " + str(e) + " -- " + str(msg.payload))
            else:
                # Value is JSON, so use {key: value, ...}
                print("  Proccessing " + str(json_value) + " as a Dict")
                es_body = {"topic": msg.topic, "timestamp": datetime.utcnow()}
                es_body.update(json_value)
                print(es_body)
                es.index(index="oort_sensors-{}".format(today), body=es_body)

    except Exception as error:
        print("*** ERROR: '{}' was unable to be inserted. Error was: {}".format(msg.payload, str(error)))
        print(traceback.format_exc())


client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message
client.username_pw_set(mqttUser, password=mqttPass)
client.connect(mqttServer, mqttPort, 60)

# Blocking call that processes network traffic, dispatches callbacks and
# handles reconnecting.
# Other loop*() functions are available that give a threaded interface and a
# manual interface.
client.loop_forever()