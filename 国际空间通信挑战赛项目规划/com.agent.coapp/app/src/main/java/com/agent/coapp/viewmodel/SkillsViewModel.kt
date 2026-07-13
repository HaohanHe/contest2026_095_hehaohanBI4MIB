package com.agent.coapp.viewmodel

import android.app.Application
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.agent.coapp.data.Skill
import com.agent.coapp.repository.ConfigRepository
import com.agent.coapp.repository.DeviceRepository
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.launch

/**
 * 技能ViewModel
 */
class SkillsViewModel(application: Application) : AndroidViewModel(application) {

    private val configRepository = ConfigRepository(application)
    private val deviceRepository = DeviceRepository()

    private val _skills = MutableStateFlow<List<Skill>>(emptyList())
    val skills: StateFlow<List<Skill>> = _skills

    private val _isLoading = MutableStateFlow(false)
    val isLoading: StateFlow<Boolean> = _isLoading

    private val _message = MutableStateFlow("")
    val message: StateFlow<String> = _message

    private val _showPushDialog = MutableStateFlow(false)
    val showPushDialog: StateFlow<Boolean> = _showPushDialog

    fun loadSkills() {
        viewModelScope.launch {
            _isLoading.value = true
            val config = configRepository.configFlow.first()

            if (config.deviceIp.isEmpty()) {
                _message.value = "请先配置设备IP地址"
                _isLoading.value = false
                return@launch
            }

            deviceRepository.getSkills(config.deviceIp, config.devicePort)
                .onSuccess { _skills.value = it }
                .onFailure { _message.value = "获取技能列表失败: ${it.message}" }

            _isLoading.value = false
        }
    }

    /**
     * 推送技能（name + 完整 markdown content）
     */
    fun pushSkill(name: String, content: String) {
        viewModelScope.launch {
            _isLoading.value = true
            val config = configRepository.configFlow.first()

            deviceRepository.pushSkill(config.deviceIp, config.devicePort, name, content)
                .onSuccess {
                    _message.value = "技能推送成功"
                    loadSkills()
                }
                .onFailure { _message.value = "推送失败: ${it.message}" }

            _isLoading.value = false
            _showPushDialog.value = false
        }
    }

    fun deleteSkill(skillName: String) {
        viewModelScope.launch {
            _isLoading.value = true
            val config = configRepository.configFlow.first()

            deviceRepository.deleteSkill(config.deviceIp, config.devicePort, skillName)
                .onSuccess {
                    _message.value = "技能已删除"
                    loadSkills()
                }
                .onFailure { _message.value = "删除失败: ${it.message}" }

            _isLoading.value = false
        }
    }

    fun showPushDialog() { _showPushDialog.value = true }
    fun hidePushDialog() { _showPushDialog.value = false }
    fun clearMessage() { _message.value = "" }
}
