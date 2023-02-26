# BookusBrick
BierBot compatible brewing and fermentation controller based upon an ESP32 board.

This project is still in an early stage and I'm still developing code.
I decided to code an additional BierBot compatible brick as I has some additional requirements next to the existing ones (see : [GitHub Bernhard Schlegel](https://github.com/BernhardSchlegel/)). 

### The Name
One of my hobbies (guess what?) is brewing beer. I do this together with my son. Our hobby-brewery is called "Bookes Beer". Hence the name of the brick :-).

### Supports
As I planned to use the BierBot system (as a starter) for an fermentation temperature chamber. So I made this list...
* Multiple actuators (at least 2)
* 1 DS18B20 temperature sensor
* Display support : for now a simple I2C 16x2 LCD display I had laying around, showing
* Current temperature
* Heating/Cooling status
* Operational errors
* On/offline status
* Compressor delay
* Temperature Set-point (using BierBot LCD-API maybe)
* Fallback temperature control in case of being offline
* Configurable using a single C include file

### Future
* Support for NTC based temperature sensors 
* Add beeper

### Pictures
Some pictures of my prototype:



