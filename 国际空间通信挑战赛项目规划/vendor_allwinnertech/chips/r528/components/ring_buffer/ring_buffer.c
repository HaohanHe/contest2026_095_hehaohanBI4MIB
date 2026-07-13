#include <errno.h>
#include <string.h>
#include <ring_buffer.h>

int ring_buffer_init(ring_buffer_t *buf, void *buf_addr, size_t buf_len)
{
	if (!buf || !buf_addr || buf_len > RING_BUFFER_LEN_MAX)
		return -EINVAL;

	buf->addr = buf_addr;
	buf->len = buf_len;
	buf->write_mirror = 0;
	buf->write_ptr = 0;
	buf->read_mirror = 0;
	buf->read_ptr = 0;

	return 0;
}

void ring_buffer_reset_ptrs(ring_buffer_t *buf)
{
	buf->write_mirror = 0;
	buf->write_ptr = 0;
	buf->read_mirror = 0;
	buf->read_ptr = 0;
}

size_t ring_buffer_writable_size(const ring_buffer_t *buf)
{
	ssize_t write_ptr = buf->write_ptr;
	ssize_t read_ptr = buf->read_ptr;
	ssize_t diff;

	if (ring_buffer_is_full(buf))
		return 0;
	if (ring_buffer_is_empty(buf))
		return buf->len;

	diff = read_ptr - write_ptr;
	if (diff < 0)
		diff += buf->len;

	return diff;
}

size_t ring_buffer_readable_size(const ring_buffer_t *buf)
{
	ssize_t write_ptr = buf->write_ptr;
	ssize_t read_ptr = buf->read_ptr;
	ssize_t diff;

	if (ring_buffer_is_full(buf))
		return buf->len;
	if (ring_buffer_is_empty(buf))
		return 0;

	diff = write_ptr - read_ptr;
	if (diff < 0)
		diff += buf->len;

	return diff;
}

static void ring_buffer_write_ptr_forward_nocheck(ring_buffer_t *buf, size_t forward_len)
{
	size_t ptr = buf->write_ptr;

	ptr += forward_len;
	if (ptr >= buf->len) {
		ptr -= buf->len;
		buf->write_mirror = ~buf->write_mirror;
	}
	buf->write_ptr = ptr;
}

static void ring_buffer_read_ptr_forward_nocheck(ring_buffer_t *buf, size_t forward_len)
{
	size_t ptr = buf->read_ptr;

	ptr += forward_len;
	if (ptr >= buf->len) {
		ptr -= buf->len;
		buf->read_mirror = ~buf->read_mirror;
	}
	buf->read_ptr = ptr;
}

#if 0
static ssize_t ring_buffer_write_ptr_forward(ring_buffer_t *buf, size_t forward_len)
{
	size_t writable_size = ring_buffer_writable_size(buf);

	if (writable_size == 0)
		return 0;

	if (forward_len > writable_size)
		forward_len = writable_size;

	ring_buffer_write_ptr_forward_nocheck(buf, forward_len);

	return forward_len;
}

static ssize_t ring_buffer_read_ptr_forward(ring_buffer_t *buf, size_t forward_len)
{
	size_t readable_size = ring_buffer_readable_size(buf);

	if (readable_size == 0)
		return 0;

	if (forward_len > readable_size)
		forward_len = readable_size;

	ring_buffer_read_ptr_forward_nocheck(buf, forward_len);

	return forward_len;
}
#endif

static void ring_buffer_write_simple(ring_buffer_t *buf, const uint8_t *data, size_t data_len)
{
	size_t nowrap_size, wrap_size;

	nowrap_size = buf->len - buf->write_ptr;
	if (nowrap_size >= data_len) {
		memcpy(buf->addr + buf->write_ptr, data, data_len);
	} else {
		wrap_size = data_len - nowrap_size;
		memcpy(buf->addr + buf->write_ptr, data, nowrap_size);
		memcpy(buf->addr, data + nowrap_size, wrap_size);
	}
}

static void ring_buffer_read_simple(ring_buffer_t *buf, uint8_t *data, size_t data_len)
{
	size_t nowrap_size, wrap_size;

	nowrap_size = buf->len - buf->read_ptr;
	if (nowrap_size >= data_len) {
		memcpy(data, buf->addr + buf->read_ptr, data_len);
	} else {
		wrap_size = data_len - nowrap_size;
		memcpy(data, buf->addr + buf->read_ptr, nowrap_size);
		memcpy(data + nowrap_size, buf->addr, wrap_size);
	}
}

ssize_t ring_buffer_write(ring_buffer_t *buf, const uint8_t *data, size_t data_len)
{
	size_t writable_size = ring_buffer_writable_size(buf);

	if (writable_size == 0)
		return 0;

	if (data_len > writable_size)
		data_len = writable_size;

	ring_buffer_write_simple(buf, data, data_len);

	ring_buffer_write_ptr_forward_nocheck(buf, data_len);
	return data_len;
}

ssize_t ring_buffer_read(ring_buffer_t *buf, uint8_t *data, size_t data_len)
{
	size_t readable_size = ring_buffer_readable_size(buf);

	if (readable_size == 0)
		return 0;

	if (data_len > readable_size)
		data_len = readable_size;

	ring_buffer_read_simple(buf, data, data_len);

	ring_buffer_read_ptr_forward_nocheck(buf, data_len);
	return data_len;
}

void ring_buffer_buf_to_buf_memcpy_simple(
		ring_buffer_t *read_buf, ring_buffer_t *write_buf, size_t data_len)
{
	size_t read_nowrap, write_nowrap;
	size_t nowrap_diff;
	uint8_t *read_addr = read_buf->addr + read_buf->read_ptr;
	uint8_t *write_addr = write_buf->addr + write_buf->write_ptr;

	read_nowrap = read_buf->len - read_buf->read_ptr;
	write_nowrap = write_buf->len - write_buf->write_ptr;

	if (read_nowrap >= data_len && write_nowrap >= data_len) {
		memcpy(write_addr, read_addr, data_len);
	} else if (read_nowrap < data_len && write_nowrap >= data_len) {
		memcpy(write_addr, read_addr, read_nowrap);
		memcpy(write_addr + read_nowrap, read_buf->addr, data_len - read_nowrap);
	} else if (read_nowrap >= data_len && write_nowrap < data_len) {
		memcpy(write_addr, read_addr, write_nowrap);
		memcpy(write_buf->addr, read_addr + write_nowrap, data_len - write_nowrap);
	} else {
		if (read_nowrap <= write_nowrap) {
			nowrap_diff = write_nowrap - read_nowrap;
			memcpy(write_addr, read_addr, read_nowrap);
			memcpy(write_addr + read_nowrap, read_buf->addr, nowrap_diff);
			memcpy(write_buf->addr, read_buf->addr + nowrap_diff,
					data_len - write_nowrap);
		} else {
			nowrap_diff = read_nowrap - write_nowrap;
			memcpy(write_addr, read_addr, write_nowrap);
			memcpy(write_buf->addr, read_addr + write_nowrap, nowrap_diff);
			memcpy(write_buf->addr + nowrap_diff, read_buf->addr,
					data_len - read_nowrap);
		}
	}
}
