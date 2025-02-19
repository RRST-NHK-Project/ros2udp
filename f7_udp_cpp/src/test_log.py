import pandas as pd
import numpy as np
import random 
import time 
from datetime import datetime, timedelta

def generate_test_log(rate):
    data = []
    base_time = datetime.now()

    for i in range(rate):

        time_stamp = base_time - timedelta(seconds=random.randint(0, 3600))

        x = random.uniform(1500, 6500)
        y = random.uniform(1000, 7000)

        orientation = random.uniform(0,360)

        param1 = random.randint(0,100)
        param2 = random.randint(0,100)
        param3 = random.randint(0,100)
        param4 = random.randint(0,100)

        shoot_state = random.randint(0,1)
        dribble_state = random.randint(0,1)

        data.append([time_stamp.strftime('%Y-%m-%d %H:%M:%S.%f'), x, y, orientation, param1, param2, param3, param4, shoot_state, dribble_state])

    data_frame = pd.DataFrame(data, columns=['Time', 'X', 'Y', 'Orientation', 'Param1', 'Param2', 'Param3', 'Param4', 'ShootState', 'DribbleState'])

    return data_frame
    
rate = 10000
test_log = generate_test_log(rate)
test_log.to_csv('test_log.csv', index=False)


