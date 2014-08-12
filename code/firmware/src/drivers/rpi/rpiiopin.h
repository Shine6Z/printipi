#ifndef DRIVERS_RPI_RPIIOPIN_H
#define DRIVERS_RPI_RPIIOPIN_H

#include "drivers/iopin.h" //for IoPin
#include "drivers/rpi/rpi.h" //for initIO
#include "bcm2835.h" //for bcm2835_*

namespace drv {
namespace rpi {

template <uint8_t PinIdx> class RpiIoPin : public IoPin {
	public:
		RpiIoPin() {
			initIO();
		}
		void makeDigitalOutput(IoLevel lev) {
			bcm2835_gpio_fsel(PinIdx, BCM2835_GPIO_FSEL_OUTP); //configure this pin as output
			digitalWrite(lev);
		}
		void makeDigitalInput() {
			bcm2835_gpio_fsel(PinIdx, BCM2835_GPIO_FSEL_INPT); //configure this pin as input
		}
		IoLevel digitalRead() const {
			return bcm2835_gpio_lev(PinIdx) ? IoHigh : IoLow;
		}
		void digitalWrite(IoLevel lev) {
			bcm2835_gpio_write(PinIdx, lev == IoHigh ? HIGH : LOW);
		}

};


}
}
#endif
