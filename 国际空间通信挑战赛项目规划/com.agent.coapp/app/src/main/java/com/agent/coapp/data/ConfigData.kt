package com.agent.coapp.data

/**
 * Agent配置数据类
 * 字段与设备端 config_store key 一一对应
 */
data class AgentConfig(
    // LLM
    val llmApiKey: String = "",          // api_key
    val llmModel: String = "",           // model
    val llmHost: String = "",            // llm_host
    val llmPath: String = "",            // llm_path
    // 搜索
    val tavilyKey: String = "",          // tavily_key
    // 火山引擎 ASR/TTS
    val volcAppKey: String = "",         // volc_appkey
    val volcToken: String = "",          // volc_token
    val volcApiKey: String = "",         // volc_api_key
    val volcCluster: String = "",        // volc_cluster
    val volcAsrCluster: String = "",     // volc_asr_cluster
    // 代理
    val proxyHost: String = "",          // proxy_host
    val proxyPort: String = "",          // proxy_port
    // 设备连接（App本地，不推手表）
    val deviceIp: String = "",
    val devicePort: Int = 28789
) {
    /** 转为设备端 key-value map（用于 PUT /api/config） */
    fun toDeviceMap(): Map<String, String> = mapOf(
        "api_key" to llmApiKey,
        "model" to llmModel,
        "llm_host" to llmHost,
        "llm_path" to llmPath,
        "tavily_key" to tavilyKey,
        "volc_appkey" to volcAppKey,
        "volc_token" to volcToken,
        "volc_api_key" to volcApiKey,
        "volc_cluster" to volcCluster,
        "volc_asr_cluster" to volcAsrCluster,
        "proxy_host" to proxyHost,
        "proxy_port" to proxyPort
    )

    companion object {
        /** 从设备端 JSON map 构建 AgentConfig（合并默认值） */
        fun fromDeviceMap(
            deviceMap: Map<String, String>,
            defaults: Map<String, String> = emptyMap(),
            localIp: String = "",
            localPort: Int = 28789
        ): AgentConfig {
            fun get(key: String): String =
                deviceMap[key]?.takeIf { it.isNotEmpty() }
                    ?: defaults[key] ?: ""

            return AgentConfig(
                llmApiKey = get("api_key"),
                llmModel = get("model"),
                llmHost = get("llm_host"),
                llmPath = get("llm_path"),
                tavilyKey = get("tavily_key"),
                volcAppKey = get("volc_appkey"),
                volcToken = get("volc_token"),
                volcApiKey = get("volc_api_key"),
                volcCluster = get("volc_cluster"),
                volcAsrCluster = get("volc_asr_cluster"),
                proxyHost = get("proxy_host"),
                proxyPort = get("proxy_port"),
                deviceIp = localIp,
                devicePort = localPort
            )
        }
    }
}
