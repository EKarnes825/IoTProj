import boto3
import json
from boto3.dynamodb.conditions import Key
from decimal import Decimal

def clean_item(item):
    return {k: int(v) if isinstance(v, Decimal) else v for k, v in item.items()}

def lambda_handler(event, context):
    dynamodb = boto3.resource('dynamodb')
    table = dynamodb.Table('ESP32-Data')

    device_id = event.get('queryStringParameters', {}).get('deviceId', 'esp32')

    response = table.query(
        KeyConditionExpression=Key('deviceId').eq(device_id),
        ScanIndexForward=True
    )

    items = [clean_item(item) for item in response['Items']]
    items.sort(key=lambda x: x['timestamp'])

    return {
        'statusCode': 200,
        'headers': {'Access-Control-Allow-Origin': '*'},
        'body': json.dumps(items)
    }
