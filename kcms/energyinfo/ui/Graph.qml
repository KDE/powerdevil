/*
 * SPDX-FileCopyrightText: 2015 David Edmundson <david@davidedmundson.co.uk>
 * SPDX-FileCopyrightText: 2025 Ismael Asensio <isma.af@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 */

import QtQuick
import org.kde.kirigami as Kirigami

/**
 * We need to draw a graph, all other libs are not suitable as we are basically
 * a connected scatter plot with non linear X spacing.
 * Currently this is not available in kdeclarative nor kqtquickcharts
 *
 * We only paint once, so canvas is fast enough for our purposes.
 * It is designed to look identical to those in ksysguard.
 */

Canvas
{
    id: canvas

    antialiasing: true

    property list<point> points

    property int yMin: 0
    property int yMax: 100
    property int yStep: 20

    property var yLabel: ( value => value )  // A formatter function

    property int xDuration: 3600

    readonly property rect plot: Qt.rect(yLabelMetrics.labelWidth,
                                         yLabelMetrics.height / 2,
                                         width - yLabelMetrics.labelWidth - 5,
                                         height - yLabelMetrics.height * 3.5)
    readonly property point plotCenter: Qt.point(
        Math.round((plot.left + plot.right) / 2),
        Math.round((plot.top + plot.bottom) / 2))

    onPointsChanged: {
        canvas.requestPaint();
    }

    onXDurationChanged: {
        canvas.requestPaint();
    }

    Behavior on xDuration {
        NumberAnimation {
            duration: Kirigami.Units.longDuration
            easing.type: Easing.OutQuad
        }
    }

    function scalePoint(point : point, currentUnixTime : int) : point {
        const scaledX = Math.round((point.x - (currentUnixTime - xDuration)) / xDuration * plot.width);
        const scaledY = Math.round((point.y - yMin) * plot.height / (yMax - yMin));

        return Qt.point(
            plot.left + scaledX,
            plot.bottom - scaledY
        );
    }

    // Spacing between x division lines, in seconds
    function stepForDuration(seconds : int) : int {
        const hours = seconds / 3600;
        if (hours <= 1) {
            return 60 * 10;      // Ten minutes
        } else if (hours <= 12) {
            return 60 * 30;      // Half an hour
        } else if (hours <= 24) {
            return 60 * 60;      // Full hour
        } else if (hours <= 48) {
            return 60 * 60 * 2;  // Two hours
        } else {
            return 60 * 60 * 12; // Full day
        }
    }

    // Offset to align the tick marks from current time
    function offsetForStep(step : int) : int {
        const now = new Date();
        const hours = now.getHours();
        const minutes = now.getMinutes();
        const seconds = now.getSeconds();

        switch (step / 60) {  // step in minutes
            case 60 * 12:
                return (hours - 12) * 3600 + minutes * 60 + seconds;
            case 60 * 2:
            case 60:
                return minutes * 60 + seconds;
            case 30:
                return (minutes - 30) * 60 + seconds;
            case 10:
                return (minutes % 10) * 60 + seconds;
            default:
                return 0;
        }
    }

    SystemPalette {
        id: palette;
        colorGroup: SystemPalette.Active

        onPaletteChanged: {
            canvas.requestPaint();
        }
    }

    TextMetrics {
        id: yLabelMetrics
        text: canvas.yLabel(canvas.yMax)

        readonly property int labelWidth: width + 10  // Adds the right spacing
    }

    onAvailableChanged: {
        if (available) {
            const c = canvas.getContext('2d');
            yLabelMetrics.font = c.font;
        }
    }

    onPaint: {
        const c = canvas.getContext('2d');

        c.clearRect(0, 0, width, height)

        c.lineWidth = 2;
        c.lineJoin = 'round';
        c.lineCap = 'round';
        c.fillStyle = palette.text

        const lineColor = palette.accent
        c.strokeStyle = lineColor;
        const gradient = c.createLinearGradient(0, 0, 0, height);
        gradient.addColorStop(0, Qt.alpha(lineColor, 0.2));
        gradient.addColorStop(1, Qt.alpha(lineColor, 0.05));
        c.fillStyle = gradient;

        // For scaling
        const currentUnixTime = Date.now() / 1000  // ms to s

        c.beginPath();

        // Draw the data points in the graph
        let firstPoint = null;
        let point = null;
        for (const dataPoint of canvas.points) {
            if (!dataPoint || dataPoint.x < currentUnixTime - xDuration) {
                continue;
            }

            if (!firstPoint) {
                firstPoint = scalePoint(dataPoint, currentUnixTime);
                c.moveTo(firstPoint.x, firstPoint.y);
                continue;
            }

            point = scalePoint(dataPoint, currentUnixTime);
            c.lineTo(point.x, point.y);
        }

        // Finish the graph area
        if (point && firstPoint) {
            c.stroke();
            c.strokeStyle = 'transparent';
            c.lineTo(point.x, plot.bottom);
            c.lineTo(firstPoint.x, plot.bottom);
            c.fill();
        }

        c.closePath()

        // Draw the frame on top

        //draw an outline
        c.strokeStyle = Qt.rgba(0, 50, 0, 0.02);
        c.lineWidth = 1;
        c.rect(plot.left, plot.top, plot.width, plot.height);

        // Draw the Y value texts
        c.fillStyle = palette.text;
        c.textAlign = "right"
        c.textBaseline = "middle";

        for(let i = 0; i <= yMax; i += yStep) {
            const y = scalePoint(Qt.point(0, i)).y;

            c.fillText(canvas.yLabel(i), plot.left - 10, y);

            //grid line
            c.moveTo(plot.left, y)
            c.lineTo(plot.width + plot.left, y)
        }
        c.stroke()

        // Draw the X value texts
        c.textAlign = "center"
        c.lineWidth = 1
        c.strokeStyle = Qt.alpha(palette.text, 0.15)

        const xStep = stepForDuration(xDuration)
        const xDivisions = xDuration / xStep
        const xGridDistance = plot.width / xDivisions
        const bottomPadding = canvas.height - plot.bottom

        const currentDayTime = new Date()

        const xOffset = offsetForStep(xStep)
        const xGridOffset = plot.width * (xOffset / xDuration)

        const dashedLines = 50
        const dashedLineLength = plot.height / dashedLines

        let lastDateStr = currentDayTime.toLocaleDateString(Qt.locale(), Locale.ShortFormat)
        let dateChanged = false

        for (let i = xDivisions; i >= -1; i--) {
            const xTickPos = i * xGridDistance + plot.left - xGridOffset

            if ((xTickPos > plot.left) && (xTickPos < plot.width + plot.left)) {
                const xTickDateTime = new Date((currentUnixTime - (xDivisions - i) * xStep - xOffset) * 1000)
                const xTickDateStr = xTickDateTime.toLocaleDateString(Qt.locale(), Locale.ShortFormat)
                const xTickTimeStr = xTickDateTime.toLocaleTimeString(Qt.locale(), Locale.ShortFormat)

                if (lastDateStr != xTickDateStr) {
                    dateChanged = true
                }

                let dashedLineDutyCycle = 0.1
                if  ((i % 2 == 0) || (xDivisions < 10)) {
                    // Display the time
                    c.fillText(xTickTimeStr, xTickPos, plot.bottom + bottomPadding / 2)

                    // If the date has changed and is not the current day in a <= 24h graph, display it
                    // Always display the date for 48h and 1 week graphs
                    if (dateChanged || xDuration > 60*60*48) {
                        c.fillText(xTickDateStr, xTickPos, plot.bottom + bottomPadding * 4/5)
                        dateChanged = false
                    }

                    // Tick markers
                    c.moveTo(xTickPos, plot.bottom)
                    c.lineTo(xTickPos, plot.bottom + bottomPadding * 1/5)

                    dashedLineDutyCycle = 0.5
                }

                for (let j = 0; j < dashedLines; j++) {
                    c.moveTo(xTickPos, plot.top + j * dashedLineLength)
                    c.lineTo(xTickPos, plot.top + (j + dashedLineDutyCycle) * dashedLineLength)
                }

                lastDateStr = xTickDateStr
            }
        }
        c.stroke()
    }
}
