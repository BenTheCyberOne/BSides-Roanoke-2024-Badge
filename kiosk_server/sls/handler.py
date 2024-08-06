"""
Defines a default puzzle server.  Created for Roanoke BSides 2024.
"""
import base64
import csv
import hashlib
import hmac
import json
import logging
import os
import string
import textwrap
import time
import random

from io import StringIO
from datetime import datetime

import boto3
import requests
import pytz

from tabulate import tabulate

from lib import part_handler

#
# Just this one helper
#
def load_secrets_file(fname):
    """
    Attempts to load a secrets file

    If one couldn't be loaded, it'll return an empty list
    """
    try:
        with open(fname, encoding="utf8") as fin:
            result = [token.strip() for token in fin.readlines()]
    except:
        print("oh no, missing ", fname)
        result = []
    return result

#
# Globals
#

logger = logging.getLogger()
logger.setLevel("INFO")

SIGN_KEY = hashlib.sha256(b"H-O-T-T-O-G-O"
                          b"Snap and clap and touch your toes"
                          b"Raise your hands, now body roll"
                          b"Dance it out, you're hot to go").hexdigest().encode("utf8")
KEY_TIMEOUT = 300 # 5 minutes


TEAM_LOAD_URL = "https://ctf-submission.console.virginiacyberrange.net/v1/" \
        "submissions/statistics/competitions/"\
        "UUID/teams"
TEAM_LOAD_PARAMS = {
        "limit": 300,
        }

aws_region = os.environ.get("AWS_REGION", "us-east-1")
s3_client = boto3.client('s3')
BUCKET_NAME = "2024badgesite"

tz = pytz.timezone('US/Eastern')

sqs = boto3.resource('sqs')
score_queue = sqs.get_queue_by_name(QueueName="scoreboard2024")

scanlist = load_secrets_file("secrets/scanlist")
badgelist = load_secrets_file("secrets/badgelist")
#answers = {answer:{"name": name, **value}
#                   for name,value in puzzles.items()
#                   for answer in value.get("answers", [])}
#
dynamodb = boto3.resource('dynamodb')
badges_table = dynamodb.Table('badges2024')
users_table = dynamodb.Table('users2024')

SAFE_CHARS = string.ascii_letters + string.digits + " -_()=?!"

CORS_HEADERS = {
      'Access-Control-Allow-Origin': '*',
      'Access-Control-Allow-Credentials': True,
      'Access-Control-Allow-Headers': '*'
      }
WEBHOOK = "WEBHOOK"
greetings = [
        "Wes hal!",
        "Eala!",
        "Freoðo!",
        "Hwæt!",
        "Eala, min frea!",
        "Freoðo, min wine!",
        "Dia dhuit, a chara!",
        "Failte!"]

TTL_LIMIT = 60*60*24*4 # 4 days in seconds

#
# More helpers
#
def load_all_items(table):
    """
    Loads everything within a given table
    """
    response = table.scan()
    data = response['Items']
    while 'LastEvaluatedKey' in response:
        response = table.scan(ExclusiveStartKey=response['LastEvaluatedKey'])
        data.extend(response['Items'])

    return data

def error(msg="Malformed request"):
    """ Quick way to return an error, and avoid timing issues """
    time.sleep(random.uniform(.2, .9))
    return {
        "statusCode": 401,
        "body": json.dumps({"error": msg}),
        "headers": {**CORS_HEADERS}
    }

def webhook(msg):
    """ Write something out to a webhook """
    requests.post(WEBHOOK, json={
            "content": msg,
            "embeds": None,
            "attachments": []},
                  timeout=3)

def make_token(badge_id, ttl=KEY_TIMEOUT, sign_key=SIGN_KEY):
    """
    Creates a token tied to a badge id, with a defined timeout.
    """
    ds = {
        "id": badge_id,
        "ttl": int(time.time())+ttl
        }
    packed = base64.b64encode(json.dumps(ds).encode("utf8"))
    signature = base64.b64encode(hmac.digest(sign_key, packed,
                                             hashlib.sha256))
    return packed + b'.' + signature


def validate_token(badge_id, token, sign_key=SIGN_KEY):
    """
    Validates a token from make_token to ensure it's in the right timeframe
    and has the corresponding badge id.

    It will fail if:
    * Token not formatted correctly
    * badge_id does not match token
    * TTL in token exceeds current time
    * signature doesn't match

    Returns a boolean without explaination
    """
    try:
        if not token:
            logger.info("Rejecting %s for not being a string", badge_id)
            return False
        if isinstance(token,bytes):
            token = token.decode()
        if "." not in token or token.count(".") != 1:
            return False
        pdata,psig = token.split(".")
        data = json.loads(base64.b64decode(pdata))
        if data['id'] != badge_id:
            logger.info("ID in token (%s) not what is being operated on (%s)",
                        data['id'], badge_id)
            return False
        if data['ttl'] <= time.time():
            logger.info("Bad ttl for %s", badge_id)
            return False
        nsig = base64.b64encode(hmac.digest(sign_key,
                                            pdata.encode("utf8"),
                                            hashlib.sha256))
        if psig.encode("utf8") == nsig:
            logger.info("Good signature for %s", badge_id)
            return True
        logger.info("Bad signature for %s, %s ", badge_id, nsig)
        logger.info(psig)
        logger.info(nsig)
    except Exception as e:
        logger.info("Exception for %s: %s", badge_id, str(e))
        return False
    return False

#
# Routes
#
def echo(event, _):
    """Defines simple response to ensure things are working"""
    response = {
        "statusCode": 200,
        "body": textwrap.dedent("""
                    Server version: Apache Tomcat/8.0.14
                    Server built:   Jul 12 2024 08:57:15
                    Server number:  8.0.14.0
                    OS Name:        AIX 5.3
                    OS Version:     5300-09-02-0849
                    Architecture:   mips
                    JVM Version:    1.7.0_55-b13
                    """),
        "headers": {**CORS_HEADERS}
    }
    return response

def get_teams(event, _):
    """ Runs periodically to pull down teams, and writes to teams.json """
    # break after the event.
    # Saturday, July 13, 2024 2:20:22 AM GMT-04:00
    if time.time() > 1720837222:
        return

    results = {}
    teams = []
    while (len(results) == 0 or "nextPageToken" in results):
        if "nextPageToken" in results:
            tlp = {**TEAM_LOAD_PARAMS,
                    "token": results['nextPageToken']}
        else:
            tlp = TEAM_LOAD_PARAMS
        response = requests.get(TEAM_LOAD_URL, params=tlp)
        results = response.json()
        for team in results.get("items", []):
            teams.append({
                "teamId": team.get("teamId"),
                "teamName": team.get("teamName")
                })
    s3_client.put_object(
        Body=json.dumps(teams),
        Bucket=BUCKET_NAME,
        Key='teams.json'
    )

def dump(event, _):
    """
    POST /dump

    Takes payload of:
    {
      "badge_id": ...
      "parts": [
      "A0DEADA0BEEF", # handshake table
      "0002EA",       # worm table
      "90135313",     # scan table
      ]
    }

    If all goes well, should return payload of:
    {
      "username": ...
      "teamId": ...
      "scoreOld": ... (int of old score)
      "scoreNew": ... (int of new score)
      "token":  ...
    }
    """
    try:
        data = json.loads(event['body'])
    except:
        logger.info("malformed payload")
        return error("Malformed payload")

    # validate
    if "parts" not in data or len(data['parts']) != 3 or "badge_id" not in data:
        return error("Malformed Payload")


    logger.info(data)
    # TODO: Validate badge_id is in our list

    # decode parts
    badge_id = data['badge_id']
    new_hst = data['parts'][0]
    new_wt = data['parts'][1]
    new_st = data['parts'][2]
    # TODO: add in badge list to the following call
    hst_data = part_handler.decode_handshake_table(new_hst, badgelist)
    wt_data = part_handler.decode_worm_table(new_wt)
    st_data = part_handler.decode_scan_table(new_st, scanlist)

    if "error" in hst_data or "error" in wt_data or "error" in st_data or \
            hst_data.get('unique') == 0:
        logger.info("Received errors\nhst: %s\n\nwt: %s\n\nst: %s",
                    hst_data, wt_data, st_data)
        return error("malformed payload")

    if badgelist and len(badge_id) > 2 and not badge_id[2:] in badgelist:
        print(badge_id)
        return error("malformed payload")

    # fetch current user data
    response = users_table.get_item(Key={'badge_id': badge_id})
    user_data = response.get("Item", {})
    response = badges_table.get_item(Key={'badge_id': badge_id})
    badge_data = response.get("Item", {})

    # compare if badge data exists
    update = True
    if badge_data:
        if not (new_hst.startswith(badge_data['hst']) and \
                new_wt.startswith(badge_data['wt']) and \
                new_st.startswith(badge_data['st'])):
            logger.info("Failed badge state for %s, dumping data",
                        badge_id)
            logger.info("HST: %s, old(%s) new(%s)",
                        new_hst.startswith(badge_data['hst']),
                        badge_data['hst'], new_hst)
            logger.info("WT: %s, old(%s) new(%s)",
                        new_wt.startswith(badge_data['wt']),
                        badge_data['wt'], new_wt)
            logger.info("HST: %s, old(%s) new(%s)",
                        new_st.startswith(badge_data['st']),
                        badge_data['st'], new_st)
            return error("Invalid badge state")

        # is there any change?
        if hst_data['packed_hst'] == badge_data['hst'] and \
           wt_data['packed_wt'] == badge_data['wt'] and \
           st_data['packed_st'] == badge_data['st']:
            update = False

    if update:
        logger.info("Updating dynamo and s3")
        # update needed?
        badge_update = {
                "badge_id": badge_id,
                "hst": hst_data['packed_hst'],
                "wt": wt_data['packed_wt'],
                "st": st_data['packed_st'],
                }
        badges_table.put_item(Item=badge_update)
        user_update = {
                "badge_id": badge_id,
                "teamId": user_data.get("teamId", ""),
                "username": user_data.get("username", ""),
                "score": hst_data['unique'] + len(st_data['scans']),
                "scoreTime": int(time.time()),
                }

        users_table.put_item(Item=user_update)
        s3_client.put_object(
            Body=json.dumps(badge_update),
            Bucket="bsidesbadgedump",
            Key=f"{int(time.time())}-{badge_id}.json"
        )
        score_queue.send_message(MessageBody=json.dumps(user_update))

    # craft token
    return {
        "statusCode": 200,
        "body": json.dumps({
              "username": user_data.get("username", ""),
              "teamId": user_data.get("teamId", ""),
              "scoreOld": int(user_data.get("score", 0)),
              "scoreNew": hst_data['unique'] + len(st_data['scans']),
              "token": make_token(badge_id).decode('utf8'),
            }),
        "headers": {**CORS_HEADERS}
        }

def change_user(event,context):
    """
    POST /change_user_data

    Takes payload of:
    {
      "token": ....
      "badge_id": ...
      "username": ...
      "teamId": ...
    }

    username and teamId are options, but one must exist.

    Username must conform to safe characters, which are defined by:
        SAFE_CHARS = string.ascii_letters + string.digits + " -_()=?!"

    Returns dict of {"message": "message"}
    """
    try:
        data = json.loads(event['body'])
    except:
        logger.info("malformed payload")
        return {
            "statusCode": 401,
            "body": json.dumps({"message": "no"}),
            "headers": {**CORS_HEADERS}
        }

    # filter to avoid shenanigans
    data = {k:v for k,v in data.items()
            if k in ("token", "badge_id", "username", "teamId")}

    if not ("token" in data and "badge_id" in data and \
            validate_token(data['badge_id'], data['token'])):
        logger.info("Bad token %s", data)
        return {
            "statusCode": 401,
            "body": json.dumps({"message": "no"}),
            "headers": {**CORS_HEADERS}
        }
    if "username" not in data or "teamId" not in data:
        logger.info("Bad payload %s", data)
        return {
            "statusCode": 400,
            "body": json.dumps({"message": "no"}),
            "headers": {**CORS_HEADERS}
        }

    # should be good to continue, fetch and update.
    response = users_table.get_item(Key={'badge_id': data['badge_id']})
    fetched = response.get("Item", {})
    updates = {}
    for var in ('username', 'teamId'):
        if var in data and data[var] != fetched.get(var):
            updates[var] = data[var]
    if updates:
        logger.info("Attribute updates %s", {f":{k}":v for k,v in updates.items()})
        users_table.update_item(Key={'badge_id': data['badge_id']},
            UpdateExpression="set " + ", ".join(f"{k}=:{k}" for k in updates.keys()),
            ExpressionAttributeValues={f":{k}":v for k,v in updates.items()})
        return {
            "statusCode": 200,
            "body": json.dumps({"message": "Updated"}),
            "headers": {**CORS_HEADERS}
        }
    return {
        "statusCode": 200,
        "body": json.dumps({"message": "No update"}),
        "headers": {**CORS_HEADERS}
    }


def update_scoreboard(event, context):
    """
    Triggered sqs queue to re-read dynamo to update scoreboard.

    Writes to scores.json and secret_board.csv.

    scores (stably sorted by scoreTime, then score):
    [{"username": string, "teamId": string, "score": number, "scoreTime": number}]


    Loads all of the users_table and teams.json

    secret_board.csv:
    Team Name, Max Badge Score

    """
    # load all table
    score_items = load_all_items(users_table)
    # {badge_id: {badge_id: "",
    #             teamId: "",
    #             username: "",
    #             score: Decimal,
    #             scoreTime: Decimal}
    score_items = sorted(score_items,
                         key=lambda k: (k['score'], -k['scoreTime']),
                         reverse=True)

    # decimal to int
    score_items = [{k:int(v) if k.startswith("score") else v
                    for k,v in score.items()}
                   for score in score_items]

    # load in teams
    teams = json.loads(s3_client.get_object(Bucket=BUCKET_NAME,
                                            Key='teams.json')["Body"].read().decode())
    teamID_name = {team['teamId']:team['teamName'] for team in teams}

    # update scores.json
    s3_client.put_object(
        Body=json.dumps([{k:v
                          for k,v in score.items()
                          if k != 'badge_id'}
                         for score in score_items]),
        Bucket=BUCKET_NAME,
        Key="scores.json"
    )

    tweeks = []

    for teamId, teamName in teamID_name.items():
        score = 0
        for user in score_items:
            if user['teamId'] == teamId:
                score = max(score, user['score'])
        tweeks.append({"Team Name": teamName, "Max Badge Score": score})

    csv_buff = StringIO()
    csv_writer = csv.DictWriter(csv_buff, list(tweeks[0].keys()))
    csv_writer.writeheader()
    for team in tweeks:
        csv_writer.writerow(team)

    s3_client.put_object(
        Body=csv_buff.getvalue(),
        Bucket=BUCKET_NAME,
        Key="secret_scoreboard.csv"
    )

    update_msgs = []
    for event in event.get("Records", []):
        if "body" not in event:
            continue
        try:
            body = json.loads(event['body'])
        except:
            continue
        update_msgs.append(f"* Score for {body['badge_id']}/{body['username']}. Now has {body['score']} points.")

    top_scorers = {'badge_id':[],
                   'username': [],
                   'teamName': [],
                   'score': [],
                   'scoreTime':[]}
    for i in range(min(len(score_items), 5)):
        for k in top_scorers.keys():
            if k == "scoreTime":
                dt = datetime.utcfromtimestamp(score_items[i][k])
                logger.info(dt)
                dt = dt.astimezone(tz)
                logger.info(dt)
                top_scorers[k].append(dt.strftime('%m-%d %H:%M:%S'))
            elif k != "teamName":
                top_scorers[k].append(score_items[i][k])
            elif score_items[i]['teamId'] in teamID_name:
                top_scorers[k].append(teamID_name[score_items[i]['teamId']])
            else:
                top_scorers[k].append("Invalid Team ID")

    webhook(f"Score update:\n{'\n'.join(update_msgs)}\n\n"
            f"**Top {len(top_scorers['badge_id'])} Scores of {len(score_items)}**\n"
            f"```\n{tabulate(top_scorers)}\n```\n")
