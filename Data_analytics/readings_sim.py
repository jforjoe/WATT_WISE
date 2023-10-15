import numpy as np
import pandas as pd

supply_voltage = 230 #Voltage
frequency = 50 #Hz


# ENERGY METER READINGS
def simulate_energy_meter(duration,fan_percentage,freq='H'):
    power_rating = 75  # Watts
    initial_power_factor = 0.9 
    years = 10  # Number of years

    if freq == 'H':
        total_hours = years * 365 * 24  # Total hours in 10 years
        degradation_factor = 0.1 / total_hours  # 10% reduction over 10 years
    elif freq == 'min':
        total_minutes = years * 365* 24 * 60 # Total minutes in 10 years
        degradation_factor = 0.1 / total_minutes  # 10% reduction over 10 years

    energy_readings = {
        'voltage': [],
        'active_power': [],
        'reactive_power': [],
        'apparent_power': [],
        'power_factor': []
    }


    fan_on = False

    last_power_factor =0

    for time in range(duration):


        if not fan_on and np.random.rand() < fan_percentage:
            fan_on = True
            fan_start = time  # Record the start time of fan operation
            last_power_factor = current_power_factor = max(0, initial_power_factor - time * degradation_factor)

        if fan_on and time >= fan_start + np.random.randint(1, 6):
            fan_on = False





        if fan_on:
            current_power_factor = max(0, initial_power_factor - time * degradation_factor)
            active_power = power_rating * current_power_factor
            reactive_power = np.sqrt(power_rating**2 - active_power**2)
            apparent_power = power_rating

        else:
            active_power = 0
            reactive_power = 0
            apparent_power = 0
            current_power_factor = last_power_factor


        energy_readings['active_power'].append(active_power)
        energy_readings['reactive_power'].append(reactive_power)
        energy_readings['apparent_power'].append(apparent_power)
        energy_readings['power_factor'].append(current_power_factor)

    return energy_readings



# Time_stamp for data generation
start_date_time = pd.to_datetime('1990-01-01')
end_date_time = pd.to_datetime('2023-12-31')

Time_stamp = pd.date_range(start_date_time,end_date_time, freq='H')


#fan operation percentage 
fan_percentage = 0.2



# Call the function to simulate energy meter readings
readings = simulate_energy_meter(len(Time_stamp),fan_percentage,freq='H')   # Total hours = len(Time_stamp)



# Create a DataFrame
data=[]

a_p, ap_p = 0, 0
for t,i,j,k in zip(Time_stamp, readings['active_power'], readings['apparent_power'], readings['power_factor']):
    a_p += i
    ap_p += j
    data.append([t, a_p, ap_p, k])



df= pd.DataFrame(data, columns=['Time_stamp', 'active_power','apparent_power','power_factor'])



#Save the data to the CSV file
df.to_csv('data.csv',index=False)
