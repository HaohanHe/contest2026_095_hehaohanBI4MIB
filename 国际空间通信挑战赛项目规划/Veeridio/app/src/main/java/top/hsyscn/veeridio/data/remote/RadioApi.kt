package top.hsyscn.veeridio.data.remote

import com.google.gson.Gson
import com.google.gson.reflect.TypeToken
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.withContext
import okhttp3.MediaType.Companion.toMediaType
import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.RequestBody.Companion.toRequestBody
import okhttp3.logging.HttpLoggingInterceptor
import top.hsyscn.veeridio.data.model.DeviceConfig
import top.hsyscn.veeridio.data.model.LogsResponse
import top.hsyscn.veeridio.data.model.Result
import top.hsyscn.veeridio.data.model.Skill
import top.hsyscn.veeridio.data.model.SkillsResponse
import java.io.IOException
import java.util.concurrent.TimeUnit

class RadioApi(baseUrl: String = "http://192.168.4.1:28790") {

    private val gson = Gson()
    private var currentBaseUrl: String = baseUrl.trimEnd('/')

    private val client: OkHttpClient = OkHttpClient.Builder()
        .connectTimeout(10, TimeUnit.SECONDS)
        .readTimeout(30, TimeUnit.SECONDS)
        .writeTimeout(30, TimeUnit.SECONDS)
        .addInterceptor(HttpLoggingInterceptor().apply {
            level = HttpLoggingInterceptor.Level.BASIC
        })
        .build()

    fun updateBaseUrl(baseUrl: String) {
        currentBaseUrl = baseUrl.trimEnd('/')
    }

    private fun url(path: String): String = "$currentBaseUrl$path"

    suspend fun getConfig(): Result<DeviceConfig> = withRetry {
        val request = Request.Builder()
            .url(url("/api/config"))
            .get()
            .build()
        safeExecute(request) { response ->
            val body = response.body?.string() ?: "{}"
            val type = object : TypeToken<Map<String, String>>() {}.type
            gson.fromJson<DeviceConfig>(body, type)
        }
    }

    suspend fun putConfig(config: DeviceConfig): Result<Unit> = withRetry {
        val json = gson.toJson(config)
        val body = json.toRequestBody(JSON_MEDIA_TYPE)
        val request = Request.Builder()
            .url(url("/api/config"))
            .put(body)
            .build()
        safeExecute(request) { }
    }

    suspend fun getSkills(): Result<List<Skill>> = withRetry {
        val request = Request.Builder()
            .url(url("/api/skills"))
            .get()
            .build()
        safeExecute(request) { response ->
            val body = response.body?.string() ?: "{\"skills\":[]}"
            gson.fromJson(body, SkillsResponse::class.java).skills
        }
    }

    suspend fun getSkill(name: String): Result<Skill> = withRetry {
        val request = Request.Builder()
            .url(url("/api/skills/$name"))
            .get()
            .build()
        safeExecute(request) { response ->
            gson.fromJson(response.body?.string(), Skill::class.java)
        }
    }

    suspend fun createOrUpdateSkill(name: String, content: String): Result<Unit> = withRetry {
        val json = gson.toJson(mapOf("name" to name, "content" to content))
        val body = json.toRequestBody(JSON_MEDIA_TYPE)
        val request = Request.Builder()
            .url(url("/api/skills"))
            .post(body)
            .build()
        safeExecute(request) { }
    }

    suspend fun deleteSkill(name: String): Result<Unit> = withRetry {
        val request = Request.Builder()
            .url(url("/api/skills/$name"))
            .delete()
            .build()
        safeExecute(request) { }
    }

    suspend fun getLogs(): Result<List<String>> = withRetry {
        val request = Request.Builder()
            .url(url("/api/logs"))
            .get()
            .build()
        safeExecute(request) { response ->
            val body = response.body?.string() ?: "{\"logs\":[]}"
            gson.fromJson(body, LogsResponse::class.java).logs
        }
    }

    suspend fun sendPtt(action: String): Result<Unit> = withRetry {
        val json = gson.toJson(mapOf("action" to action))
        val body = json.toRequestBody(JSON_MEDIA_TYPE)
        val request = Request.Builder()
            .url(url("/api/ptt"))
            .post(body)
            .build()
        safeExecute(request) { }
    }

    private inline fun <T> safeExecute(request: Request, block: (okhttp3.Response) -> T): Result<T> {
        return try {
            client.newCall(request).execute().use { response ->
                if (!response.isSuccessful) {
                    return Result.Error("HTTP ${response.code}")
                }
                Result.Success(block(response))
            }
        } catch (e: IOException) {
            Result.Error("网络错误: ${e.message}")
        } catch (e: Exception) {
            Result.Error("意外错误: ${e.message}")
        }
    }

    private suspend inline fun <T> withRetry(
        maxRetries: Int = 3,
        crossinline block: suspend () -> Result<T>
    ): Result<T> {
        var lastError: Result.Error? = null
        for (attempt in 0 until maxRetries) {
            val result = block()
            if (result is Result.Success) return result
            lastError = result as Result.Error
            if (attempt < maxRetries - 1) {
                delay((500L * (attempt + 1)).coerceAtMost(2000L))
            }
        }
        return lastError ?: Result.Error("未知错误")
    }

    companion object {
        private val JSON_MEDIA_TYPE = "application/json; charset=utf-8".toMediaType()
    }
}
