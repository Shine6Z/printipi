/* The MIT License (MIT)
 *
 * Copyright (c) 2014 Colin Wallace
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
 

#ifndef IODRIVERS_TEMPCONTROL_H
#define IODRIVERS_TEMPCONTROL_H

#include <utility> //for std::move

#include "iodriver.h"
#include "common/filters/nofilter.h"
#include "common/intervaltimer.h"
#include "platforms/auto/chronoclock.h" //for EventClockT
#include "iodrivers/iopin.h"
#include "compileflags.h" //for CelciusType
#include "pid.h" //for default template parameter
#include "common/logging.h"

namespace iodrv {

//enum passed to instructor to define the TempControl instance as either controlling a Hotend or a Heated Bed.
//Functionally, they work the same, but each type responds to different G-codes.
enum TempControlType {
    HotendType,
    HeatedBedType
};

/* 
 * TempControl provides a way to coordinate thermistor readings with the PWM control of a hotend OR heated bed.
 * It used a PID controller to determine the ideal PWM for a given thermistor reading and temperature target.
 * Additionally, it accepts an (optional) filter applied BEFORE the PID controller, which can be used to weed out some variability in readings (use a low-pass filter for this).
 * Currently, it assumes an RC-based thermistor, but in the future it may be expanded to work with any analog IoPin too.
 */
template <typename Thermistor, typename PID=PID, typename Filter=NoFilter> class TempControl : public IODriver {
    TempControlType _hotType;
    IoPin _heater;
    Thermistor _therm;
    PID _pid;
    Filter _filter;
    EventClockT::duration _pwmPeriod;
    EventClockT::duration _pwmUpdateInterval;

    float _destTemp;
    float _filteredTemp;
    bool _isReading;
    EventClockT::time_point _nextPwmUpdate;
    public:
        inline TempControl(TempControlType hotType, IoPin &&heater, Thermistor &&therm, 
            const PID &pid, const Filter &filter, EventClockT::duration pwmPeriod=std::chrono::microseconds(40), EventClockT::duration pwmUpdateInterval=std::chrono::milliseconds(200)) 
         : IODriver(), _hotType(hotType), _heater(std::move(heater)), _therm(std::move(therm)), _pid(pid), _filter(filter), 
         _pwmPeriod(pwmPeriod), _pwmUpdateInterval(pwmUpdateInterval), 
         _destTemp(mathutil::ABSOLUTE_ZERO_CELCIUS), _filteredTemp(mathutil::ABSOLUTE_ZERO_CELCIUS), 
         _isReading(false), _nextPwmUpdate(EventClockT::now()) {
            _heater.setDefaultState(IO_DEFAULT_LOW);
            _heater.makePwmOutput(0.0);
        }
        //register as the correct device type
        inline bool isHotend() const {
            return _hotType == HotendType;
        }
        inline bool isHeatedBed() const {
            return _hotType == HeatedBedType;
        }
        inline void setTargetTemperature(CelciusType t) {
            _destTemp = t;
        }
        inline CelciusType getMeasuredTemperature() const {
            return _filteredTemp; //don't return _therm.value(), as that's unfiltered
        }
        inline CelciusType getTargetTemperature() const {
            return _destTemp;
        }
        inline bool onIdleCpu(OnIdleCpuIntervalT interval) {
            bool needMoreCpu = _therm.onIdleCpu(interval);
            //periodically, map temperature to hotend power feedback & update pwm.
            //Note: even updating on the wide intervals alone may be too much if the platform's pwmWrite() function is expensive like on rpi
            if (interval == OnIdleCpuIntervalWide && EventClockT::now() >= _nextPwmUpdate) {
                _nextPwmUpdate += _pwmUpdateInterval;
                float t = _therm.value();
                //a temperature <= absolute zero is used to signal errors.
                //TODO: raise a fatal error (machine shutdown) if thermistor read errors persist for more than, say, 10 sec.
                if (t > mathutil::ABSOLUTE_ZERO_CELCIUS) {
                    updatePwm(t);
                }
            }
            return needMoreCpu;
        }
    private:
        inline void updatePwm(float lastTemp) {
	        //Filter the measurement values to compensate for jitter, etc
            _filteredTemp = _filter.feed(lastTemp);
            //Then feed the filtered measurements to a feedback controller
            float pwm = _pid.feed(_destTemp, _filteredTemp);
            LOG("tempcontrol: drive-strength=%f, temp=%f *C\n", pwm, _filteredTemp);
            _heater.pwmWrite(pwm, _pwmPeriod);
        }
};


}
#endif
