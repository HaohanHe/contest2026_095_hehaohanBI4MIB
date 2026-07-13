package com.agent.coapp.data

/**
 * 技能数据类（对应设备端 GET /api/skills 响应）
 */
data class Skill(
    val name: String,
    val description: String = "",
    val content: String? = null,
    val file: String = "",
    val size: Long = 0,
    val mtime: String = ""
)

/**
 * 技能列表响应
 */
data class SkillsResponse(
    val skills: List<Skill>? = null
)

/**
 * 推送技能请求（POST /api/skills）
 */
data class SkillPushRequest(
    val name: String,
    val content: String
)
