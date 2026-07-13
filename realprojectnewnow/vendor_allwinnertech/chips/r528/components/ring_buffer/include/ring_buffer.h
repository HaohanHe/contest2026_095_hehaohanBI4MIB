#ifndef __RING_BUFFER_H__
#define __RING_BUFFER_H__

#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C"
{
#endif

/* write/read_ptr is 31-bit, can only index 0 ~ 2^31-1 */
#define RING_BUFFER_LEN_MAX 0x80000000


/*
 * Use the MSB of the write/read_ptr as mirror flag. We adds a virtual mirror to
 * the buffer, and the pointer point to either the normal part or mirror part.
 * If write_ptr is equal to read_ptr, but in different mirrors, the buffer is
 * full. If write_ptr and read_ptr are the same and in the same mirror, the
 * buffer is empty.
 *
 * e.g. a buffer with 7 bytes:
 *
 *          mirror = 0                    mirror = 1
 * +---+---+---+---+---+---+---+|+~~~+~~~+~~~+~~~+~~~+~~~+~~~+
 * | 0 | 1 | 2 | 3 | 4 | 5 | 6 ||| 0 | 1 | 2 | 3 | 4 | 5 | 6 | Full
 * +---+---+---+---+---+---+---+|+~~~+~~~+~~~+~~~+~~~+~~~+~~~+
 *           ^                             ^
 *       read_idx                      write_idx
 *
 * +---+---+---+---+---+---+---+|+~~~+~~~+~~~+~~~+~~~+~~~+~~~+
 * | 0 | 1 | 2 | 3 | 4 | 5 | 6 ||| 0 | 1 | 2 | 3 | 4 | 5 | 6 | Empty
 * +---+---+---+---+---+---+---+|+~~~+~~~+~~~+~~~+~~~+~~~+~~~+
 *           ^
 *       read_idx
 *       write_idx
 *
 */
typedef struct __ring_buffer {
	uint8_t *addr;
	size_t len;
	volatile uint32_t write_ptr:31;
	volatile uint32_t write_mirror:1;
	volatile uint32_t read_ptr:31;
	volatile uint32_t read_mirror:1;
} ring_buffer_t;

int ring_buffer_init(ring_buffer_t *buf, void *buf_addr, size_t buf_len);

void ring_buffer_reset_ptrs(ring_buffer_t *buf);

static inline int ring_buffer_is_empty(const ring_buffer_t *buf)
{
	return (buf->write_ptr == buf->read_ptr)
		&& (buf->write_mirror == buf->read_mirror);
}

static inline int ring_buffer_is_full(const ring_buffer_t *buf)
{
	return (buf->write_ptr == buf->read_ptr)
		&& (buf->write_mirror != buf->read_mirror);
}

size_t ring_buffer_writable_size(const ring_buffer_t *buf);
size_t ring_buffer_readable_size(const ring_buffer_t *buf);

ssize_t ring_buffer_write(ring_buffer_t *buf, const uint8_t *data, size_t data_len);
ssize_t ring_buffer_read(ring_buffer_t *buf, uint8_t *data, size_t data_len);

void ring_buffer_buf_to_buf_memcpy_simple(
		ring_buffer_t *read_buf, ring_buffer_t *write_buf, size_t data_len);

#ifdef __cplusplus
}
#endif

#endif /* ifndef __RING_BUFFER_H__ */
