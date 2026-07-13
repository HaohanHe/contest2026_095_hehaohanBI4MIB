package com.agent.coapp.viewmodel

import android.app.Application
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.agent.coapp.repository.ConfigRepository
import com.agent.coapp.repository.DeviceRepository
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.launch

/**
 * 日志ViewModel
 */
class LogsViewModel(application: Application) : AndroidViewModel(application) {
    
    private val configRepository = ConfigRepository(application)
    private val deviceRepository = DeviceRepository()
    
    private val _logs = MutableStateFlow<List<String>>(emptyList())
    val logs: StateFlow<List<String>> = _logs
    
    private val _filteredLogs = MutableStateFlow<List<String>>(emptyList())
    val filteredLogs: StateFlow<List<String>> = _filteredLogs
    
    private val _isLoading = MutableStateFlow(false)
    val isLoading: StateFlow<Boolean> = _isLoading
    
    private val _message = MutableStateFlow("")
    val message: StateFlow<String> = _message
    
    private val _filterKeyword = MutableStateFlow("")
    val filterKeyword: StateFlow<String> = _filterKeyword
    
    /**
     * 获取日志
     */
    fun loadLogs(lines: Int = 100) {
        viewModelScope.launch {
            _isLoading.value = true
            val config = configRepository.configFlow.first()
            
            if (config.deviceIp.isEmpty()) {
                _message.value = "请先配置设备IP地址"
                _isLoading.value = false
                return@launch
            }
            
            val result = deviceRepository.getLogs(config.deviceIp, config.devicePort, lines)
            result.onSuccess { logsList ->
                _logs.value = logsList
                applyFilter()
            }.onFailure { error ->
                _message.value = "获取日志失败: ${error.message}"
            }
            
            _isLoading.value = false
        }
    }
    
    /**
     * 设置过滤关键词
     */
    fun setFilterKeyword(keyword: String) {
        _filterKeyword.value = keyword
        applyFilter()
    }
    
    /**
     * 应用过滤
     */
    private fun applyFilter() {
        val keyword = _filterKeyword.value
        if (keyword.isEmpty()) {
            _filteredLogs.value = _logs.value
        } else {
            _filteredLogs.value = _logs.value.filter { log ->
                log.contains(keyword, ignoreCase = true)
            }
        }
    }
    
    /**
     * 刷新日志
     */
    fun refresh() {
        loadLogs()
    }
    
    /**
     * 清除消息
     */
    fun clearMessage() {
        _message.value = ""
    }
}
