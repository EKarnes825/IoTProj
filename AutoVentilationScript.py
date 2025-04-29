import json
import boto3

iot = boto3.client('iot-data')

THING_NAME = "ESP32"

def lambda_handler(event, context):
    tvoc = event.get('tvoc', 0)
    ventilation = 1 if tvoc > 500 else 0

    payload = {
        "state": {
            "desired": {
                "ventilation": ventilation
            }
        }
    }

    response = iot.update_thing_shadow(
        thingName=THING_NAME,
        payload=json.dumps(payload)
    )

    return {
        'statusCode': 200,
        'body': f"Shadow updated with ventilation={ventilation}"
    }
