package com.agent.coapp.repository

import android.content.Context
import androidx.datastore.core.DataStore
import androidx.datastore.preferences.core.*
import androidx.datastore.preferences.preferencesDataStore
import com.agent.coapp.data.AgentConfig
import com.google.gson.Gson
import com.google.gson.reflect.TypeToken
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.map

private val Context.dataStore: DataStore<Preferences> by preferencesDataStore(name = "agent_config")

/**
 * 配置数据仓库 — DataStore持久化
 */
class ConfigRepository(private val context: Context) {

    companion object {
        private val KEY_LLM_API_KEY = stringPreferencesKey("api_key")
        private val KEY_LLM_MODEL = stringPreferencesKey("model")
        private val KEY_LLM_HOST = stringPreferencesKey("llm_host")
        private val KEY_LLM_PATH = stringPreferencesKey("llm_path")
        private val KEY_TAVILY_KEY = stringPreferencesKey("tavily_key")
        private val KEY_VOLC_APPKEY = stringPreferencesKey("volc_appkey")
        private val KEY_VOLC_TOKEN = stringPreferencesKey("volc_token")
        private val KEY_VOLC_API_KEY = stringPreferencesKey("volc_api_key")
        private val KEY_VOLC_CLUSTER = stringPreferencesKey("volc_cluster")
        private val KEY_VOLC_ASR_CLUSTER = stringPreferencesKey("volc_asr_cluster")
        private val KEY_PROXY_HOST = stringPreferencesKey("proxy_host")
        private val KEY_PROXY_PORT = stringPreferencesKey("proxy_port")
        private val KEY_DEVICE_IP = stringPreferencesKey("device_ip")
        private val KEY_DEVICE_PORT = intPreferencesKey("device_port")
    }

    val configFlow: Flow<AgentConfig> = context.dataStore.data.map { prefs ->
        AgentConfig(
            llmApiKey = prefs[KEY_LLM_API_KEY] ?: "",
            llmModel = prefs[KEY_LLM_MODEL] ?: "",
            llmHost = prefs[KEY_LLM_HOST] ?: "",
            llmPath = prefs[KEY_LLM_PATH] ?: "",
            tavilyKey = prefs[KEY_TAVILY_KEY] ?: "",
            volcAppKey = prefs[KEY_VOLC_APPKEY] ?: "",
            volcToken = prefs[KEY_VOLC_TOKEN] ?: "",
            volcApiKey = prefs[KEY_VOLC_API_KEY] ?: "",
            volcCluster = prefs[KEY_VOLC_CLUSTER] ?: "",
            volcAsrCluster = prefs[KEY_VOLC_ASR_CLUSTER] ?: "",
            proxyHost = prefs[KEY_PROXY_HOST] ?: "",
            proxyPort = prefs[KEY_PROXY_PORT] ?: "",
            deviceIp = prefs[KEY_DEVICE_IP] ?: "",
            devicePort = prefs[KEY_DEVICE_PORT] ?: 28789
        )
    }

    suspend fun saveConfig(config: AgentConfig) {
        context.dataStore.edit { prefs ->
            prefs[KEY_LLM_API_KEY] = config.llmApiKey
            prefs[KEY_LLM_MODEL] = config.llmModel
            prefs[KEY_LLM_HOST] = config.llmHost
            prefs[KEY_LLM_PATH] = config.llmPath
            prefs[KEY_TAVILY_KEY] = config.tavilyKey
            prefs[KEY_VOLC_APPKEY] = config.volcAppKey
            prefs[KEY_VOLC_TOKEN] = config.volcToken
            prefs[KEY_VOLC_API_KEY] = config.volcApiKey
            prefs[KEY_VOLC_CLUSTER] = config.volcCluster
            prefs[KEY_VOLC_ASR_CLUSTER] = config.volcAsrCluster
            prefs[KEY_PROXY_HOST] = config.proxyHost
            prefs[KEY_PROXY_PORT] = config.proxyPort
            prefs[KEY_DEVICE_IP] = config.deviceIp
            prefs[KEY_DEVICE_PORT] = config.devicePort
        }
    }

    suspend fun updateDeviceIp(ip: String) {
        context.dataStore.edit { it[KEY_DEVICE_IP] = ip }
    }

    /**
     * 从 assets 读取默认配置 JSON
     */
    fun loadDefaultConfig(): Map<String, String> {
        return try {
            val json = context.assets.open("default_config.json").bufferedReader().readText()
            Gson().fromJson(json, object : TypeToken<Map<String, String>>() {}.type)
        } catch (e: Exception) {
            try {
                val json = context.assets.open("default_config.example.json").bufferedReader().readText()
                Gson().fromJson(json, object : TypeToken<Map<String, String>>() {}.type)
            } catch (e2: Exception) {
                emptyMap()
            }
        }
    }
}
