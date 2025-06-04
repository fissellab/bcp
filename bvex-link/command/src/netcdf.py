from netCDF4 import Dataset

def create_netcdf_file(file_path: str) -> None:
    """Create a new NetCDF file with the appropriate structure for storing primitive data."""
    with Dataset(file_path, 'w', format='NETCDF4') as nc:
        # Create dimensions
        nc.createDimension('time', None)  # Unlimited dimension for time

        # Create variables
        # Double precision float for timestamps
        timestamp_var = nc.createVariable('timestamp', 'f8', ('time',))
        # Double precision float for values
        value_var = nc.createVariable('value', 'f8', ('time',))

        # Add attributes to variables
        timestamp_var.units = 'seconds since 1970-01-01 00:00:00'
        timestamp_var.long_name = 'Unix timestamp'
        value_var.long_name = 'Primitive value'


def write_to_netcdf(file_path: str, value: float | int | bool, timestamp: float) -> None:
    """Write a primitive value and its timestamp to the NetCDF file."""
    with Dataset(file_path, 'a') as nc:
        # Convert boolean to float if necessary
        if isinstance(value, bool):
            value = float(value)

        # Get the current size of the time dimension
        time_size = len(nc.dimensions['time'])

        # Write the new data
        nc.variables['timestamp'][time_size] = timestamp
        nc.variables['value'][time_size] = value