// SPDX-License-Identifier: LGPL-2.1-or-later
/*
 * This file is part of libnvme.
 * Copyright (c) 2021 Code Construct Pty Ltd
 *
 * Authors: Jeremy Kerr <jk@codeconstruct.com.au>
 */

/**
 * DOC: mi.h - NVMe Management Interface library (libnvme-mi) definitions.
 *
 * These provide an abstraction for the MI messaging between controllers
 * and a host, typically over an MCTP-over-i2c link to a NVMe device, used
 * as part of the out-of-band management of a system.
 *
 * We have a few data structures define here to reflect the topology
 * of a MI connection with an NVMe subsystem:
 *
 *  - &nvme_mi_ep_t: an MI endpoint - our mechanism of communication with a
 *    NVMe subsystem. For MCTP, an endpoint will be the component that
 *    holds the MCTP address (EID), and receives our request message.
 *
 *    endpoints are defined in the NVMe-MI spec, and are specific to the MI
 *    interface.
 *
 *    Each endpoint will provide access to one or more of:
 *
 *  - &nvme_mi_ctrl_t: a NVMe controller, as defined by the NVMe base spec.
 *    The controllers are responsible for processing any NVMe standard
 *    commands (eg, the Admin command set). An endpoint (&nvme_mi_ep_t)
 *    may provide access to multiple controllers - so each of the controller-
 *    type commands will require a &nvme_mi_ctrl_t to be specified, rather than
 *    an endpoint
 *
 * A couple of conventions with the libnvme-mi API:
 *
 *  - All types and functions have the nvme_mi prefix, to distinguish from
 *    the libnvme core.
 *
 *  - We currently support either MI commands and Admin commands. The
 *    former adds a _mi prefix, the latter an _admin prefix. [This does
 *    result in the MI functions having a double _mi, like
 *    &nvme_mi_mi_subsystem_health_status_poll, which is apparently amusing
 *    for our German-speaking readers]
 *
 * In line with the core NVMe API, the Admin command functions take an
 * `_args` structure to provide the command-specific parameters. However,
 * for the MI interface, the fd and timeout members of these _args structs
 * are ignored.
 *
 * References to the specifications here will either to be the NVM Express
 * Management Interface ("NVMe-MI") or the NVM Express Base specification
 * ("NVMe"). At the time of writing, the versions we're referencing here
 * are:
 *  - NVMe-MI 1.2b
 *  - NVMe 2.0b
 * with a couple of accommodations for older spec types, particularly NVMe-MI
 * 1.1, where possible.
 *
 */

#ifndef _LIBNVME_MI_MI_H
#define _LIBNVME_MI_MI_H

#include <endian.h>
#include <stdint.h>

#include "types.h"
#include "tree.h"

/**
 * NVME_MI_MSGTYPE_NVME - MCTP message type for NVMe-MI messages.
 *
 * This is defined by MCTP, but is referenced as part of the NVMe-MI message
 * spec. This is the MCTP NVMe message type (0x4), with the message-integrity
 * bit (0x80) set.
 */
#define NVME_MI_MSGTYPE_NVME 0x84

/* Basic MI message definitions */

/**
 * enum nvme_mi_message_type - NVMe-MI message type field.
 * @NVME_MI_MT_CONTROL: NVME-MI Control Primitive
 * @NVME_MI_MT_MI: NVMe-MI command
 * @NVME_MI_MT_ADMIN: NVMe Admin command
 * @NVME_MI_MT_PCIE: PCIe command
 *
 * Used as byte 1 of both request and response messages (NMIMT bits of NMP
 * byte). Not to be confused with the MCTP message type in byte 0.
 */
enum nvme_mi_message_type {
	NVME_MI_MT_CONTROL = 0,
	NVME_MI_MT_MI = 1,
	NVME_MI_MT_ADMIN = 2,
	NVME_MI_MT_PCIE = 4,
};

/**
 * enum nvme_mi_ror: Request or response field.
 * @NVME_MI_ROR_REQ: request message
 * @NVME_MI_ROR_RSP: response message
 */
enum nvme_mi_ror {
	NVME_MI_ROR_REQ = 0,
	NVME_MI_ROR_RSP = 1,
};

/**
 * struct nvme_mi_msg_hdr - General MI message header.
 * @type: MCTP message type, will always be NVME_MI_MSGTYPE_NVME
 * @nmp: NVMe-MI message parameters (including MI message type)
 * @meb: Management Endpoint Buffer flag; unused for libnvme-mi implementation
 * @rsvd0: currently reserved
 *
 * Wire format shared by both request and response messages, per NVMe-MI
 * section 3.1. This is used for all message types, MI and Admin.
 */
struct nvme_mi_msg_hdr {
	__u8	type;
	__u8	nmp;
	__u8	meb;
	__u8	rsvd0;
} __attribute__((packed));

/**
 * enum nvme_mi_mi_opcode - Operation code for supported NVMe-MI commands.
 * @nvme_mi_mi_opcode_mi_data_read: Read NVMe-MI Data Structure
 * @nvme_mi_mi_opcode_subsys_health_status_poll: Subsystem Health Status Poll
 */
enum nvme_mi_mi_opcode {
	nvme_mi_mi_opcode_mi_data_read = 0x00,
	nvme_mi_mi_opcode_subsys_health_status_poll = 0x01,
};

/**
 * struct nvme_mi_mi_req_hdr - MI request message header.
 * @hdr: generic MI message header
 * @opcode: opcode (OPC) for the specific MI command
 * @rsvd0: reserved bytes
 * @cdw0: Management Request Doubleword 0 - command specific usage
 * @cdw1: Management Request Doubleword 1 - command specific usage
 *
 * Wire format for MI request message headers, defined in section 5 of NVMe-MI.
 */
struct nvme_mi_mi_req_hdr {
	struct nvme_mi_msg_hdr hdr;
	__u8	opcode;
	__u8	rsvd0[3];
	__le32	cdw0, cdw1;
};

/**
 * struct nvme_mi_mi_resp_hdr - MI response message header.
 * @hdr: generic MI message header
 * @status: generic response status from command; non-zero on failure.
 * @nmresp: NVMe Management Response: command-type-specific response data
 *
 * Wire format for MI response message header, defined in section 5 of NVMe-MI.
 */
struct nvme_mi_mi_resp_hdr {
	struct nvme_mi_msg_hdr hdr;
	__u8	status;
	__u8	nmresp[3];
};

/**
 * enum nvme_mi_dtyp - Data Structure Type field.
 * @nvme_mi_dtyp_subsys_info: NVM Subsystem Information
 * @nvme_mi_dtyp_port_info: Port information
 * @nvme_mi_dtyp_ctrl_list: Controller List
 * @nvme_mi_dtyp_ctrl_info: Controller Information
 * @nvme_mi_dtyp_opt_cmd_support: Optionally Supported Command List
 * @nvme_mi_dtyp_meb_support: Management Endpoint Buffer Command Support List
 *
 * Data Structure Type field for Read NVMe-MI Data Structure command, used to
 * indicate the particular structure to query from the endpoint.
 */
enum nvme_mi_dtyp {
	nvme_mi_dtyp_subsys_info = 0x00,
	nvme_mi_dtyp_port_info = 0x01,
	nvme_mi_dtyp_ctrl_list = 0x02,
	nvme_mi_dtyp_ctrl_info = 0x03,
	nvme_mi_dtyp_opt_cmd_support = 0x04,
	nvme_mi_dtyp_meb_support = 0x05,
};

/* Admin command definitions */

/**
 * struct nvme_mi_admin_req_hdr - Admin command request header.
 * @hdr: Generic MI message header
 * @opcode: Admin command opcode (using enum nvme_admin_opcode)
 * @flags: Command Flags, indicating dlen and doff validity; Only defined in
 *         NVMe-MI version 1.1, no fields defined in 1.2 (where the dlen/doff
 *         are always considered valid).
 * @ctrl_id: Controller ID target of command
 * @cdw1: Submission Queue Entry doubleword 1
 * @cdw2: Submission Queue Entry doubleword 2
 * @cdw3: Submission Queue Entry doubleword 3
 * @cdw4: Submission Queue Entry doubleword 4
 * @cdw5: Submission Queue Entry doubleword 5
 * @doff: Offset of data to return from command
 * @dlen: Length of sent/returned data
 * @rsvd0: Reserved
 * @rsvd1: Reserved
 * @cdw10: Submission Queue Entry doubleword 10
 * @cdw11: Submission Queue Entry doubleword 11
 * @cdw12: Submission Queue Entry doubleword 12
 * @cdw13: Submission Queue Entry doubleword 13
 * @cdw14: Submission Queue Entry doubleword 14
 * @cdw15: Submission Queue Entry doubleword 15
 *
 * Wire format for Admin command message headers, defined in section 6 of
 * NVMe-MI.
 */
struct nvme_mi_admin_req_hdr {
	struct nvme_mi_msg_hdr hdr;
	__u8	opcode;
	__u8	flags;
	__le16	ctrl_id;
	__le32	cdw1, cdw2, cdw3, cdw4, cdw5;
	__le32	doff;
	__le32	dlen;
	__le32	rsvd0, rsvd1;
	__le32	cdw10, cdw11, cdw12, cdw13, cdw14, cdw15;
} __attribute((packed));

/**
 * struct nvme_mi_admin_resp_hdr - Admin command response header.
 * @hdr: Generic MI message header
 * @status: Generic response code, non-zero on failure
 * @rsvd0: Reserved
 * @cdw0: Completion Queue Entry doubleword 0
 * @cdw1: Completion Queue Entry doubleword 1
 * @cdw3: Completion Queue Entry doubleword 3
 *
 * This is the generic response format with the three doublewords of completion
 * queue data, plus optional response data.
 */
struct nvme_mi_admin_resp_hdr {
	struct nvme_mi_msg_hdr hdr;
	__u8	status;
	__u8	rsvd0[3];
	__le32	cdw0, cdw1, cdw3;
} __attribute__((packed));


/**
 * nvme_mi_create_root() - Create top-level MI (root) handle.
 * @fp:		File descriptor for logging messages
 * @log_level:	Logging level to use
 *
 * Create the top-level (library) handle for creating subsequent endpoint
 * objects. Similar to nvme_create_root(), but we provide this to allow linking
 * without the core libnvme.
 *
 * Return: new root object, or NULL on failure.
 *
 * See &nvme_create_root.
 */
nvme_root_t nvme_mi_create_root(FILE *fp, int log_level);

/**
 * nvme_mi_free_root() - Free root object.
 * @root: root to free
 */
void nvme_mi_free_root(nvme_root_t root);

/* Top level management object: NVMe-MI Management Endpoint */
struct nvme_mi_ep;

/**
 * typedef nvme_mi_ep_t - MI Endpoint object.
 *
 * Represents our communication endpoint on the remote MI-capable device.
 * To be used for direct MI commands for the endpoint (through the
 * nvme_mi_mi_* functions(), or to communicate with individual controllers
 * (see &nvme_mi_init_ctrl).
 *
 * Endpoints are created through a transport-specific constructor; currently
 * only MCTP-connected endpoints are supported, through &nvme_mi_open_mctp.
 * Subsequent operations on the endpoint (and related controllers) are
 * transport-independent.
 */
typedef struct nvme_mi_ep * nvme_mi_ep_t;

struct nvme_mi_ctrl;

/**
 * typedef nvme_mi_ctrl_t - NVMe-MI Controller object.
 *
 * Provides NVMe command functionality, through the MI interface.
 */
typedef struct nvme_mi_ctrl * nvme_mi_ctrl_t;

/**
 * nvme_mi_open_mctp() - Create an endpoint using a MCTP connection.
 * @root: root object to create under
 * @netid: MCTP network ID on this system
 * @eid: MCTP endpoint ID
 *
 * Transport-specific endpoint initialisation for MI-connected endpoints. Once
 * an endpoint is created, the rest of the API is transport-independent.
 *
 * Return: New endpoint object for @netid & @eid, or NULL on failure.
 *
 * See &nvme_mi_close
 */
nvme_mi_ep_t nvme_mi_open_mctp(nvme_root_t root, unsigned int netid, uint8_t eid);

/**
 * nvme_mi_close() - Close an endpoint connection and release resources
 *
 * @ep: Endpoint object to close
 */
void nvme_mi_close(nvme_mi_ep_t ep);

/**
 * nvme_mi_init_ctrl() - initialise a NVMe controller.
 * @ep: Endpoint to create under
 * @ctrl_id: ID of controller to initialise.
 *
 * Create a connection to a controller behind the endpoint specified in @ep.
 * Controller IDs may be queried from the endpoint through
 * &nvme_mi_mi_read_mi_data_ctrl_list.
 *
 * Return: New controller object, or NULL on failure.
 *
 * See &nvme_mi_close_ctrl
 */
nvme_mi_ctrl_t nvme_mi_init_ctrl(nvme_mi_ep_t ep, __u16 ctrl_id);

/**
 * nvme_mi_close_ctrl() - free a controller
 * @ctrl: controller to free
 */
void nvme_mi_close_ctrl(nvme_mi_ctrl_t ctrl);

/* MI Command API: nvme_mi_mi_ prefix */

/**
 * nvme_mi_mi_read_mi_data_subsys() - Perform a Read MI Data Structure command,
 * retrieving subsystem data.
 * @ep: endpoint for MI communication
 * @s: subsystem information to populate
 *
 * Retrieves the Subsystem information - number of external ports and
 * NVMe version information. See &struct nvme_mi_read_nvm_ss_info.
 *
 * Return: 0 on success, non-zero on failure.
 */
int nvme_mi_mi_read_mi_data_subsys(nvme_mi_ep_t ep,
				   struct nvme_mi_read_nvm_ss_info *s);

/**
 * nvme_mi_mi_read_mi_data_port() - Perform a Read MI Data Structure command,
 * retrieving port data.
 * @ep: endpoint for MI communication
 * @portid: id of port data to retrieve
 * @p: port information to populate
 *
 * Retrieves the Port information, for the specified port ID. The subsystem
 * data (from &nvme_mi_mi_read_mi_data_subsys) nmp field contains the allowed
 * range of port IDs.
 *
 * See &struct nvme_mi_read_port_info.
 *
 * Return: 0 on success, non-zero on failure.
 */
int nvme_mi_mi_read_mi_data_port(nvme_mi_ep_t ep, __u8 portid,
				 struct nvme_mi_read_port_info *p);

/**
 * nvme_mi_mi_read_mi_data_ctrl_list() - Perform a Read MI Data Structure
 * command, retrieving the list of attached controllers.
 * @ep: endpoint for MI communication
 * @start_ctrlid: starting controller ID
 * @list: controller list to populate
 *
 * Retrieves the list of attached controllers, with IDs greater than or
 * equal to @start_ctrlid.
 *
 * See &struct nvme_ctrl_list.
 *
 * Return: 0 on success, non-zero on failure.
 */
int nvme_mi_mi_read_mi_data_ctrl_list(nvme_mi_ep_t ep, __u8 start_ctrlid,
				      struct nvme_ctrl_list *list);

/**
 * nvme_mi_mi_read_mi_data_ctrl() - Perform a Read MI Data Structure command,
 * retrieving controller information
 * @ep: endpoint for MI communication
 * @ctrl_id: ID of controller to query
 * @ctrl: controller data to populate
 *
 * Retrieves the Controller Information Data Structure for the attached
 * controller with ID @ctrlid.
 *
 * See &struct nvme_mi_read_ctrl_info.
 *
 * Return: 0 on success, non-zero on failure.
 */
int nvme_mi_mi_read_mi_data_ctrl(nvme_mi_ep_t ep, __u16 ctrl_id,
				 struct nvme_mi_read_ctrl_info *ctrl);

/**
 * nvme_mi_mi_subsystem_health_status_poll() - Read the Subsystem Health
 * Data Structure from the NVM subsystem
 * @ep: endpoint for MI communication
 * @clear: flag to clear the Composite Controller Status state
 * @nshds: subsystem health status data to populate
 *
 * Retrieves the Subsystem Health Data Structure into @nshds. If @clear is
 * set, requests that the Composite Controller Status bits are cleared after
 * the read. See NVMe-MI section 5.6 for details on the CCS bits.
 *
 * See &struct nvme_mi_nvm_ss_health_status.
 *
 * Return: 0 on success, non-zero on failure.
 */
int nvme_mi_mi_subsystem_health_status_poll(nvme_mi_ep_t ep, bool clear,
					    struct nvme_mi_nvm_ss_health_status *nshds);

/* Admin channel functions */

/**
 * nvme_mi_admin_xfer() -  Raw admin transfer interface.
 * @ctrl: controller to send the admin command to
 * @admin_req: request data
 * @req_data_size: size of request data payload
 * @admin_resp: buffer for response data
 * @resp_data_offset: offset into request data to retrieve from controller
 * @resp_data_size: size of response data buffer, updated to received size
 *
 * Performs an arbitrary NVMe Admin command, using the provided request data,
 * in @admin_req. The size of the request data *payload* is specified in
 * @req_data_size - this does not include the standard header length (so a
 * header-only request would have a size of 0).
 *
 * On success, response data is stored in @admin_resp, which has an optional
 * appended payload buffer of @resp_data_size bytes. The actual payload
 * transferred will be stored in @resp_data_size. These sizes do not include
 * the Admin request header, so 0 represents no payload.
 *
 * As with all Admin commands, we can request partial data from the Admin
 * Response payload, offset by @resp_data_offset.
 *
 * See: &struct nvme_mi_admin_req_hdr and &struct nvme_mi_admin_resp_hdr.
 *
 * Return: 0 on success, non-zero on failure.
 */
int nvme_mi_admin_xfer(nvme_mi_ctrl_t ctrl,
		       struct nvme_mi_admin_req_hdr *admin_req,
		       size_t req_data_size,
		       struct nvme_mi_admin_resp_hdr *admin_resp,
		       off_t resp_data_offset,
		       size_t *resp_data_size);

/**
 * nvme_mi_admin_identify_partial() - Perform an Admin identify command,
 * and retrieve partial response data.
 * @ctrl: Controller to process identify command
 * @args: Identify command arguments
 * @offset: offset of identify data to retrieve from response
 * @size: size of identify data to return
 *
 * Perform an Identify command, using the Identify command parameters in @args.
 * The @offset and @size arguments allow the caller to retrieve part of
 * the identify response. See NVMe-MI section 6.2 for the semantics (and some
 * handy diagrams) of the offset & size parameters.
 *
 * Will return an error if the length of the response data (from the controller)
 * did not match @size.
 *
 * Unless you're performing a vendor-unique identify command, You'll probably
 * want to use one of the identify helpers (nvme_mi_admin_identify,
 * nvme_mi_admin_identify_cns_nsid, or nvme_mi_admin_identify_<type>) instead
 * of this. If the type of your identify command is standardised but not
 * yet supported by libnvme-mi, please contact the maintainers.
 *
 * Return: 0 on success, non-zero on failure
 *
 * See: &struct nvme_identify_args
 */
int nvme_mi_admin_identify_partial(nvme_mi_ctrl_t ctrl,
				   struct nvme_identify_args *args,
				   off_t offset, size_t size);

/**
 * nvme_mi_admin_identify() - Perform an Admin identify command.
 * @ctrl: Controller to process identify command
 * @args: Identify command arguments
 *
 * Perform an Identify command, using the Identify command parameters in @args.
 * Stores the identify data in ->data, and (if set) the result from cdw0
 * into args->result.
 *
 * Will return an error if the length of the response data (from the
 * controller) is not a full &NVME_IDENTIFY_DATA_SIZE.
 *
 * Return: 0 on success, non-zero on failure
 *
 * See: &struct nvme_identify_args
 */
static inline int nvme_mi_admin_identify(nvme_mi_ctrl_t ctrl,
					 struct nvme_identify_args *args)
{
	return nvme_mi_admin_identify_partial(ctrl, args,
					      0, NVME_IDENTIFY_DATA_SIZE);
}

/**
 * nvme_mi_admin_identify_cns_nsid() - Perform an Admin identify command using
 * specific CNS/NSID parameters.
 * @ctrl: Controller to process identify command
 * @cns: Controller or Namespace Structure, specifying identified object
 * @nsid: namespace ID
 * @data: buffer for identify data response
 *
 * Perform an Identify command, using the CNS specifier @cns, and the
 * namespace ID @nsid if required by the CNS type.
 *
 * Stores the identify data in @data, which is expected to be a buffer of
 * &NVME_IDENTIFY_DATA_SIZE bytes.
 *
 * Will return an error if the length of the response data (from the
 * controller) is not a full &NVME_IDENTIFY_DATA_SIZE.
 *
 * Return: 0 on success, non-zero on failure
 */
static inline int nvme_mi_admin_identify_cns_nsid(nvme_mi_ctrl_t ctrl,
						  enum nvme_identify_cns cns,
						  __u32 nsid, void *data)
{
	struct nvme_identify_args args = {
		.result = NULL,
		.data = data,
		.args_size = sizeof(args),
		.cns = cns,
		.csi = NVME_CSI_NVM,
		.nsid = nsid,
		.cntid = NVME_CNTLID_NONE,
		.cns_specific_id = NVME_CNSSPECID_NONE,
		.uuidx = NVME_UUID_NONE,
	};

	return nvme_mi_admin_identify(ctrl, &args);
}

/**
 * nvme_mi_admin_identify_ctrl() - Perform an Admin identify for a controller
 * @ctrl: Controller to process identify command
 * @id: Controller identify data to populate
 *
 * Perform an Identify command, for the controller specified by @ctrl,
 * writing identify data to @id.
 *
 * Will return an error if the length of the response data (from the
 * controller) is not a full &NVME_IDENTIFY_DATA_SIZE, so @id will be
 * be fully populated on success.
 *
 * Return: 0 on success, non-zero on failure
 *
 * See: &struct nvme_id_ctrl
 */
static inline int nvme_mi_admin_identify_ctrl(nvme_mi_ctrl_t ctrl,
					      struct nvme_id_ctrl *id)
{
	return nvme_mi_admin_identify_cns_nsid(ctrl, NVME_IDENTIFY_CNS_CTRL,
					       NVME_NSID_NONE, id);
}

/**
 * nvme_mi_admin_identify_ctrl_list() - Perform an Admin identify for a
 * controller list.
 * @ctrl: Controller to process identify command
 * @cntid: Controller ID to specify list start
 * @list: List data to populate
 *
 * Perform an Identify command, for the controller list starting with
 * IDs greater than or equal to @cntid.
 *
 * Will return an error if the length of the response data (from the
 * controller) is not a full &NVME_IDENTIFY_DATA_SIZE, so @id will be
 * be fully populated on success.
 *
 * Return: 0 on success, non-zero on failure
 *
 * See: &struct nvme_ctrl_list
 */
static inline int nvme_mi_admin_identify_ctrl_list(nvme_mi_ctrl_t ctrl,
						   __u16 cntid,
						   struct nvme_ctrl_list *list)
{
	struct nvme_identify_args args = {
		.result = NULL,
		.data = list,
		.args_size = sizeof(args),
		.cns = NVME_IDENTIFY_CNS_CTRL_LIST,
		.csi = NVME_CSI_NVM,
		.nsid = NVME_NSID_NONE,
		.cntid = cntid,
		.cns_specific_id = NVME_CNSSPECID_NONE,
		.uuidx = NVME_UUID_NONE,
	};

	return nvme_mi_admin_identify(ctrl, &args);
}

/**
 * nvme_mi_admin_get_log_page() - Retrieve log page data from controller
 * @ctrl: Controller to query
 * @args: Get Log Page command arguments
 *
 * Performs a Get Log Page Admin command as specified by @args. Response data
 * is stored in @args->data, which should be a buffer of @args->data_len bytes.
 * Resulting data length is stored in @args->data_len on successful
 * command completion.
 *
 * This request may be implemented as multiple log page commands, in order
 * to fit within MI message-size limits.
 *
 * Return: 0 on success, non-zero on failure
 *
 * See: &struct nvme_get_log_args
 */
int nvme_mi_admin_get_log_page(nvme_mi_ctrl_t ctrl,
			       struct nvme_get_log_args *args);

/**
 * nvme_mi_admin_security_send() - Perform a Security Send command on a
 * controller.
 * @ctrl: Controller to send command to
 * @args: Security Send command arguments
 *
 * Performs a Security Send Admin command as specified by @args. Response data
 * is stored in @args->data, which should be a buffer of @args->data_len bytes.
 * Resulting data length is stored in @args->data_len on successful
 * command completion.
 *
 * Security Send data length should not be greater than 4096 bytes to
 * comply with specification limits.
 *
 * Return: 0 on success, non-zero on failure
 *
 * See: &struct nvme_get_log_args
 */
int nvme_mi_admin_security_send(nvme_mi_ctrl_t ctrl,
				struct nvme_security_send_args *args);

/**
 * nvme_mi_admin_security_recv() - Perform a Security Receive command on a
 * controller.
 * @ctrl: Controller to send command to
 * @args: Security Receive command arguments
 *
 * Performs a Security Receive Admin command as specified by @args. Response
 * data is stored in @args->data, which should be a buffer of @args->data_len
 * bytes. Resulting data length is stored in @args->data_len on successful
 * command completion.
 *
 * Security Receive data length should not be greater than 4096 bytes to
 * comply with specification limits.
 *
 * Return: 0 on success, non-zero on failure
 *
 * See: &struct nvme_get_log_args
 */
int nvme_mi_admin_security_recv(nvme_mi_ctrl_t ctrl,
				struct nvme_security_receive_args *args);


#endif /* _LIBNVME_MI_MI_H */
