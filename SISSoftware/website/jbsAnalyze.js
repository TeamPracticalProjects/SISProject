function analyzeSensorLog() {

    commandOutputClear();

    commandOutputAdd("This comes from the routine where I would add analysis.")

    var msg = "Sensor log has " + g_sparkCoreData.SensorLog.length + " entries in it."
    commandOutputAdd(msg);

}
