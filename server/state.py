# state.py
import time
from typing import List, Dict, Any
from config import MAX_EVENTS, MAX_RECORDS

logs = {"data": [], "fota": [], "config": [], "write": []}
data_records: List[Dict[str, Any]] = []

def push_log(bucket: str, ev: Dict[str, Any]):
    logs[bucket].append({"ts": int(time.time()*1000), **ev})
    if len(logs[bucket]) > MAX_EVENTS:
        del logs[bucket][:len(logs[bucket]) - MAX_EVENTS]

def trim_records():
    if len(data_records) > MAX_RECORDS:
        del data_records[:len(data_records) - MAX_RECORDS]
