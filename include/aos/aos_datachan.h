#ifndef LIB_AOS_DATACHAN_H
#define LIB_AOS_DATACHAN_H

#include <aos/aos_rpc.h>
#include <aos/waitset.h>

struct aos_dc_ringbuffer
{
    size_t length;
    size_t read;
    size_t write;

    char *buffer;
};

/**
 * \brief channel with a receive buffer
 * 
 */
struct aos_datachan
{
    enum aos_rpc_backend backend;

    union {
        struct lmp_chan lmp;
        struct ump_chan ump;
    } channel;

    bool is_closed;
    size_t bytes_left;

    /// receive buffer
    struct aos_dc_ringbuffer buffer;
};


void aos_dc_buffer_init(struct aos_dc_ringbuffer *buf, size_t bytes, char *buffer);


size_t aos_dc_write_into_buffer(struct aos_dc_ringbuffer *buf, size_t bytes, const char *data);
size_t aos_dc_read_from_buffer(struct aos_dc_ringbuffer *buf, size_t bytes, char *data);
size_t aos_dc_bytes_available(struct aos_dc_ringbuffer *buf);


errval_t aos_dc_init_lmp(struct aos_datachan *dc, size_t buffer_length);
errval_t aos_dc_init_ump(struct aos_datachan *dc, size_t buffer_length, lvaddr_t ump_page, size_t ump_page_size, bool first_half);
errval_t aos_dc_free(struct aos_datachan *dc);


bool aos_dc_send_is_connected(struct aos_datachan *dc);


/**
 * \brief send some data over the channel.
 * 
 * This function may block until everything can be sent.
 * 
 * \param bytes length of the data
 * \param data data itself
 */
errval_t aos_dc_send(struct aos_datachan *dc, size_t bytes, const char *data);


/**
 * \brief receive buffered data but don't block
 * 
 * This function will try to read up to the requested length from the channel's buffer. It will
 * return the number of bytes read in `received`
 */
errval_t aos_dc_receive_available(struct aos_datachan *dc, size_t bytes, char *data, size_t *received);



errval_t aos_dc_can_receive(struct aos_datachan *dc);


/**
 * \brief receive some data
 * 
 * This function blocks until it receives some data.
 */
errval_t aos_dc_receive(struct aos_datachan *dc, size_t bytes, char *data, size_t *received);

errval_t aos_dc_receive_all(struct aos_datachan *dc, size_t bytes, char *data);

/**
 * \brief register a handler to be notified whenever somethin can be received on this channel
 */
errval_t aos_dc_register(struct aos_datachan *dc, struct waitset *ws, struct event_closure closure);

errval_t aos_dc_deregister(struct aos_datachan *dc);


bool aos_dc_is_closed(struct aos_datachan *dc);

/**
 * \brief sends a close message through the channel
 * 
 * \param dc 
 * \return errval_t 
 */
errval_t aos_dc_close(struct aos_datachan *dc);

#endif // LIB_AOS_DATACHAN_H
