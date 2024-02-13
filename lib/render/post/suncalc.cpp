/*
SPDX-FileCopyrightText: 2017 Roman Gilg <subdiff@gmail.com>

SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "suncalc.h"

#include "constants.h"

#include <QDateTime>
#include <QTimeZone>
#include <QtMath>

namespace como::render::post
{

#define TWILIGHT_NAUT -12.0
#define TWILIGHT_CIVIL -6.0
#define SUN_RISE_SET -0.833
#define SUN_HIGH 2.0

static QTime convert_to_local_time(const QDateTime& when, const QTime& utcTime)
{
    const QTimeZone timeZone = QTimeZone::systemTimeZone();
    const int utcOffset = timeZone.offsetFromUtc(when);
    return utcTime.addSecs(utcOffset);
}

QPair<QDateTime, QDateTime>
calculate_sun_timings(const QDateTime& dateTime, double latitude, double longitude, bool at_morning)
{
    // calculations based on https://aa.quae.nl/en/reken/zonpositie.html
    // accuracy: +/- 5min

    // positioning
    const double rad = M_PI / 180.;
    const double earthObliquity = 23.4397; // epsilon

    const double lat = latitude;   // phi
    const double lng = -longitude; // lw

    // times
    const QDateTime utcDateTime = dateTime.toUTC();
    const double juPrompt = utcDateTime.date().toJulianDay(); // J
    const double ju2000 = 2451545.;                           // J2000

    // geometry
    auto mod360 = [](double number) -> double { return std::fmod(number, 360.); };

    auto sin = [&rad](double angle) -> double { return std::sin(angle * rad); };
    auto cos = [&rad](double angle) -> double { return std::cos(angle * rad); };
    auto asin = [&rad](double val) -> double { return std::asin(val) / rad; };
    auto acos = [&rad](double val) -> double { return std::acos(val) / rad; };

    auto anomaly = [&](const double date) -> double { // M
        return mod360(357.5291 + 0.98560028 * (date - ju2000));
    };

    auto center = [&sin](double anomaly) -> double { // C
        return 1.9148 * sin(anomaly) + 0.02 * sin(2 * anomaly) + 0.0003 * sin(3 * anomaly);
    };

    auto ecliptLngMean = [](double anom)
        -> double { // Mean ecliptical longitude L_sun = Mean Anomaly + Perihelion + 180°
        return anom + 282.9372; // anom + 102.9372 + 180°
    };

    auto ecliptLng = [&](double anom) -> double { // lambda = L_sun + C
        return ecliptLngMean(anom) + center(anom);
    };

    auto declination = [&](const double date) -> double { // delta
        const double anom = anomaly(date);
        const double eclLng = ecliptLng(anom);

        return mod360(asin(sin(earthObliquity) * sin(eclLng)));
    };

    // sun hour angle at specific angle
    auto hourAngle = [&](const double date, double angle) -> double { // H_t
        const double decl = declination(date);
        const double ret0 = (sin(angle) - sin(lat) * sin(decl)) / (cos(lat) * cos(decl));

        double ret = mod360(acos(ret0));
        if (180. < ret) {
            ret = ret - 360.;
        }
        return ret;
    };

    /*
     * Sun positions
     */

    // transit is at noon
    auto getTransit = [&](const double date) -> double { // Jtransit
        const double juMeanSolTime
            = juPrompt - ju2000 - 0.0009 - lng / 360.; // n_x = J - J_2000 - J_0 - l_w / 360°
        const double juTrEstimate
            = date + qRound64(juMeanSolTime) - juMeanSolTime; // J_x = J + n - n_x
        const double anom = anomaly(juTrEstimate);            // M
        const double eclLngM = ecliptLngMean(anom);           // L_sun

        return juTrEstimate + 0.0053 * sin(anom) - 0.0068 * sin(2 * eclLngM);
    };

    auto getSunMorning = [&hourAngle](const double angle, const double transit) -> double {
        return transit - hourAngle(transit, angle) / 360.;
    };

    auto getSunEvening = [&hourAngle](const double angle, const double transit) -> double {
        return transit + hourAngle(transit, angle) / 360.;
    };

    /*
     * Begin calculations
     */

    // noon - sun at the highest point
    const double juNoon = getTransit(juPrompt);

    double begin, end;
    if (at_morning) {
        begin = getSunMorning(TWILIGHT_CIVIL, juNoon);
        end = getSunMorning(SUN_HIGH, juNoon);
    } else {
        begin = getSunEvening(SUN_HIGH, juNoon);
        end = getSunEvening(TWILIGHT_CIVIL, juNoon);
    }
    // transform to QDateTime
    begin += 0.5;
    end += 0.5;

    QDateTime dateTimeBegin;
    QDateTime dateTimeEnd;

    if (!std::isnan(begin)) {
        const double dayFraction = begin - int(begin);
        const QTime utcTime = QTime::fromMSecsSinceStartOfDay(dayFraction * MSC_DAY);
        const QTime localTime = convert_to_local_time(dateTime, utcTime);
        dateTimeBegin = QDateTime(dateTime.date(), localTime);
    }

    if (!std::isnan(end)) {
        const double dayFraction = end - int(end);
        const QTime utcTime = QTime::fromMSecsSinceStartOfDay(dayFraction * MSC_DAY);
        const QTime localTime = convert_to_local_time(dateTime, utcTime);
        dateTimeEnd = QDateTime(dateTime.date(), localTime);
    }

    return {dateTimeBegin, dateTimeEnd};
}

}
