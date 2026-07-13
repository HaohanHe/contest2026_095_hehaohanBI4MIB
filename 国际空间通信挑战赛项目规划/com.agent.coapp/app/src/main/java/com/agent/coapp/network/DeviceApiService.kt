package com.agent.coapp.network

import com.agent.coapp.data.Skill
import com.agent.coapp.data.SkillPushRequest
import com.agent.coapp.data.SkillsResponse
import com.google.gson.Gson
import com.google.gson.reflect.TypeToken
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import okhttp3.*
import okhttp3.MediaType.Companion.toMediaType
import okhttp3.RequestBody.Companion.toRequestBody
import java.io.IOException
import java.util.concurrent.TimeUnit

/**
 * 设备API服务
 * 处理与Agent设备的HTTP通信
 */
class DeviceApiService {

    private val client = OkHttpClient.Builder()
        .connectTimeout(10, TimeUnit.SECONDS)
        .readTimeout(15, TimeUnit.SECONDS)
        .writeTimeout(15, TimeUnit.SECONDS)
        .build()

    private val gson = Gson()

    companion object {
        private val JSON_MEDIA_TYPE = "application/json; charset=utf-8".toMediaType()
    }

    /**
     * GET /api/config → Map<String, String>
     */
    suspend fun getConfig(baseUrl: String): Result<Map<String, String>> = withContext(Dispatchers.IO) {
        try {
            val request = Request.Builder()
                .url("$baseUrl/api/config")
                .get()
                .build()

            val response = client.newCall(request).execute()
            if (response.isSuccessful) {
                val body = response.body?.string() ?: "{}"
                val map: Map<String, String> = gson.fromJson(
                    body, object : TypeToken<Map<String, String>>() {}.type
                )
                Result.success(map)
            } else {
                Result.failure(IOException("获取配置失败: ${response.code}"))
            }
        } catch (e: Exception) {
            Result.failure(e)
        }
    }

    /**
     * PUT /api/config with key-value map
     */
    suspend fun updateConfig(baseUrl: String, config: Map<String, String>): Result<Unit> = withContext(Dispatchers.IO) {
        try {
            val json = gson.toJson(config)
            val requestBody = json.toRequestBody(JSON_MEDIA_TYPE)

            val request = Request.Builder()
                .url("$baseUrl/api/config")
                .put(requestBody)
                .build()

            val response = client.newCall(request).execute()
            if (response.isSuccessful) {
                Result.success(Unit)
            } else {
                Result.failure(IOException("更新配置失败: ${response.code}"))
            }
        } catch (e: Exception) {
            Result.failure(e)
        }
    }

    /**
     * GET /api/skills → List<Skill>
     */
    suspend fun getSkills(baseUrl: String): Result<List<Skill>> = withContext(Dispatchers.IO) {
        try {
            val request = Request.Builder()
                .url("$baseUrl/api/skills")
                .get()
                .build()

            val response = client.newCall(request).execute()
            if (response.isSuccessful) {
                val body = response.body?.string() ?: "{}"
                val resp = gson.fromJson(body, SkillsResponse::class.java)
                Result.success(resp.skills ?: emptyList())
            } else {
                Result.failure(IOException("获取技能列表失败: ${response.code}"))
            }
        } catch (e: Exception) {
            Result.failure(e)
        }
    }

    /**
     * POST /api/skills with name + content
     */
    suspend fun pushSkill(baseUrl: String, skill: SkillPushRequest): Result<Unit> = withContext(Dispatchers.IO) {
        try {
            val json = gson.toJson(skill)
            val requestBody = json.toRequestBody(JSON_MEDIA_TYPE)

            val request = Request.Builder()
                .url("$baseUrl/api/skills")
                .post(requestBody)
                .build()

            val response = client.newCall(request).execute()
            if (response.isSuccessful) {
                Result.success(Unit)
            } else {
                Result.failure(IOException("推送技能失败: ${response.code}"))
            }
        } catch (e: Exception) {
            Result.failure(e)
        }
    }

    /**
     * DELETE /api/skills/{name}
     */
    suspend fun deleteSkill(baseUrl: String, skillName: String): Result<Unit> = withContext(Dispatchers.IO) {
        try {
            val request = Request.Builder()
                .url("$baseUrl/api/skills/$skillName")
                .delete()
                .build()

            val response = client.newCall(request).execute()
            if (response.isSuccessful) {
                Result.success(Unit)
            } else {
                Result.failure(IOException("删除技能失败: ${response.code}"))
            }
        } catch (e: Exception) {
            Result.failure(e)
        }
    }

    /**
     * GET /api/logs
     */
    suspend fun getLogs(baseUrl: String, lines: Int = 100): Result<List<String>> = withContext(Dispatchers.IO) {
        try {
            val request = Request.Builder()
                .url("$baseUrl/api/logs?lines=$lines")
                .get()
                .build()

            val response = client.newCall(request).execute()
            if (response.isSuccessful) {
                val body = response.body?.string() ?: "{}"
                val map: Map<String, Any> = gson.fromJson(
                    body, object : TypeToken<Map<String, Any>>() {}.type
                )
                @Suppress("UNCHECKED_CAST")
                val logs = (map["logs"] as? List<String>) ?: emptyList()
                Result.success(logs)
            } else {
                Result.failure(IOException("获取日志失败: ${response.code}"))
            }
        } catch (e: Exception) {
            Result.failure(e)
        }
    }
}
