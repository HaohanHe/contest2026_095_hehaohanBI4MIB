package top.hsyscn.veeridio.data.preferences

import android.content.Context
import androidx.datastore.core.DataStore
import androidx.datastore.preferences.core.Preferences
import androidx.datastore.preferences.core.edit
import androidx.datastore.preferences.core.intPreferencesKey
import androidx.datastore.preferences.core.stringPreferencesKey
import androidx.datastore.preferences.preferencesDataStore
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.map
import top.hsyscn.veeridio.data.model.DeviceInfo

private val Context.dataStore: DataStore<Preferences> by preferencesDataStore(name = "device_prefs")

class DevicePreferences(context: Context) {

    private val dataStore = context.dataStore

    val deviceInfo: Flow<DeviceInfo?> = dataStore.data.map { prefs ->
        val name = prefs[KEY_NAME] ?: return@map null
        val address = prefs[KEY_ADDRESS] ?: return@map null
        val ip = prefs[KEY_IP] ?: return@map null
        val port = prefs[KEY_PORT] ?: 28790
        DeviceInfo(name, address, ip, port)
    }

    val deviceIp: Flow<String?> = dataStore.data.map { it[KEY_IP] }

    suspend fun saveDevice(info: DeviceInfo) {
        dataStore.edit { prefs ->
            prefs[KEY_NAME] = info.name
            prefs[KEY_ADDRESS] = info.address
            prefs[KEY_IP] = info.ip
            prefs[KEY_PORT] = info.port
        }
    }

    suspend fun saveIp(ip: String) {
        dataStore.edit { prefs ->
            prefs[KEY_IP] = ip
        }
    }

    suspend fun clear() {
        dataStore.edit { it.clear() }
    }

    companion object {
        private val KEY_NAME = stringPreferencesKey("device_name")
        private val KEY_ADDRESS = stringPreferencesKey("device_address")
        private val KEY_IP = stringPreferencesKey("device_ip")
        private val KEY_PORT = intPreferencesKey("device_port")
    }
}
