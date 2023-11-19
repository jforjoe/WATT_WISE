import requests

# Set the API key
API_KEY = "e6fbd971b70e4482bfe460776236e764"

# Set the latitude and longitude of Chennai
LAT = 13.077778
LON = 80.274444

# Set the start and end dates
START_DATE = "1990-01-01"
END_DATE = "2022-12-31"

# Make the API request
url = f"https://api.weatherbit.io/v2.0/history/subhourly?lat={LAT}&lon={LON}&start_date={START_DATE}&end_date={END_DATE}&key={API_KEY}"
response = requests.get(url)

# Check if the request was successful
if response.status_code == 200:
  # Parse the JSON response
  data = response.json()

  # Extract the temperature readings
  temperature_readings = []
  for record in data['data']:
    temperature_readings.append(record['temp'])

  # Print the temperature readings
  print(temperature_readings)
else:
  # Raise an exception if the request was not successful
  raise Exception("API request failed")