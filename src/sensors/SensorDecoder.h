#pragma once

#include <QList>
#include <QVariantMap>

#include <vector>

struct SensorPulse
{
    bool high = false;
    int durationUs = 0;
};

class SensorDecoder
{
public:
    QList<QVariantMap> decode(const std::vector<SensorPulse> &pulses,
                              double frequencyHz,
                              double signalLevel) const;
};
