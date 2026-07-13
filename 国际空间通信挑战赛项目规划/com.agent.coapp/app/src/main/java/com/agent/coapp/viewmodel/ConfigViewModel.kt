package com.agent.coapp.viewmodel

import android.app.Application
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.agent.coapp.data.AgentConfig
import com.agent.coapp.repository.ConfigRepository
import com.agent.coapp.repository.DeviceRepository
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.launch

/**
 * 配置ViewModel
 * 同步策略：手表是 source of truth，手机默认配置是补丁
 */
class ConfigViewModel(application: Application) : AndroidViewModel(application) {

    private val configRepository = ConfigRepository(application)
    private val deviceRepository = DeviceRepository()

    val configFlow: StateFlow<AgentConfig> = MutableStateFlow(AgentConfig())

    private val _isLoading = MutableStateFlow(false)
    val isLoading: StateFlow<Boolean> = _isLoading

    private val _syncResult = MutableStateFlow<SyncResult?>(null)
    val syncResult: StateFlow<SyncResult?> = _syncResult

    init {
        viewModelScope.launch {
            configRepository.configFlow.collect { config ->
                (configFlow as MutableStateFlow).value = config
            }
        }
    }

    fun saveConfig(config: AgentConfig) {
        viewModelScope.launch { configRepository.saveConfig(config) }
    }

    /**
     * 从手表拉配置 → 合并默认值 → 保存本地
     */
    fun syncFromDevice() {
        viewModelScope.launch {
            _isLoading.value = true
            val local = configRepository.configFlow.first()

            if (local.deviceIp.isEmpty()) {
                _syncResult.value = SyncResult.Error("请先配置设备IP地址")
                _isLoading.value = false
                return@launch
            }

            val defaults = configRepository.loadDefaultConfig()
            val result = deviceRepository.getConfig(local.deviceIp, local.devicePort)

            result.onSuccess { watchMap ->
                val merged = AgentConfig.fromDeviceMap(
                    deviceMap = watchMap,
                    defaults = defaults,
                    localIp = local.deviceIp,
                    localPort = local.devicePort
                )
                configRepository.saveConfig(merged)
                _syncResult.value = SyncResult.Success("配置已从设备同步")
            }.onFailure {
                // 离线时用本地 + 默认值填充空字段
                val merged = AgentConfig.fromDeviceMap(
                    deviceMap = local.toDeviceMap(),
                    defaults = defaults,
                    localIp = local.deviceIp,
                    localPort = local.devicePort
                )
                configRepository.saveConfig(merged)
                _syncResult.value = SyncResult.Error("设备离线，使用本地配置")
            }

            _isLoading.value = false
        }
    }

    /**
     * 推送配置到手表（PUT /api/config）— 用当前编辑值
     */
    fun syncToDevice(editedConfig: AgentConfig) {
        viewModelScope.launch {
            _isLoading.value = true
            configRepository.saveConfig(editedConfig)

            if (editedConfig.deviceIp.isEmpty()) {
                _syncResult.value = SyncResult.Error("请先配置设备IP地址")
                _isLoading.value = false
                return@launch
            }

            val result = deviceRepository.updateConfig(
                editedConfig.deviceIp, editedConfig.devicePort, editedConfig.toDeviceMap()
            )

            result.onSuccess {
                _syncResult.value = SyncResult.Success("配置已同步到设备")
            }.onFailure { error ->
                _syncResult.value = SyncResult.Error("同步失败: ${error.message}")
            }

            _isLoading.value = false
        }
    }

    /**
     * 推送配置到手表（PUT /api/config）— 用本地已保存值
     */
    fun syncToDevice() {
        viewModelScope.launch {
            _isLoading.value = true
            val config = configRepository.configFlow.first()

            if (config.deviceIp.isEmpty()) {
                _syncResult.value = SyncResult.Error("请先配置设备IP地址")
                _isLoading.value = false
                return@launch
            }

            val result = deviceRepository.updateConfig(
                config.deviceIp, config.devicePort, config.toDeviceMap()
            )

            result.onSuccess {
                _syncResult.value = SyncResult.Success("配置已同步到设备")
            }.onFailure { error ->
                _syncResult.value = SyncResult.Error("同步失败: ${error.message}")
            }

            _isLoading.value = false
        }
    }

    fun clearSyncResult() { _syncResult.value = null }
}

sealed class SyncResult {
    data class Success(val message: String) : SyncResult()
    data class Error(val message: String) : SyncResult()
}
