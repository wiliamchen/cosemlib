/**
 * Implementation of the Cosem ACSE services
 *
 * Copyright (c) 2016, Anthony Rabine
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms of the BSD license.
 * See LICENSE.txt for more details.
 *
 */

#include "csm_association.h"
#include "string.h"
#include "csm_axdr_codec.h"


// Since this is part of a Cosem stack, simplify the decoding to lower code & RAM ;
// Instead of performing a real decoding, just compare the memory as it is always the same
static const uint8_t cOidHeader[] = {0x60U, 0x85U, 0x74U, 0x05U, 0x08U};

// Object identifier names
#define APP_CONTEXT_NAME            1U
#define SECURITY_MECHANISM_NAME     2U


typedef enum
{
    CSM_ACSE_ERR = 0U, ///< Encoding/decoding is NOT good, stop here if it is required
    CSM_ACSE_OK = 1U    ///< Encoding/decoding is good, we can continue

} csm_acse_code;

typedef csm_acse_code (*extract_param)(csm_asso_state *state, csm_ber *ber, csm_array *array);
typedef csm_acse_code (*insert_param)(csm_asso_state *state, csm_ber *ber, csm_array *array);

enum acse_context
{
    ACSE_NONE,  //!< Never decode/encode
    ACSE_ANY,   //!< Always decode/encode
    ACSE_OPT,   //!< Optional, skiped if not exists
    ACSE_SEC,   //!< When use ciphered authentication
};

typedef struct
{
    uint8_t tag;
    uint8_t context;    //!< Requirement on the context
    extract_param extract_func;
    insert_param insert_func;
} csm_asso_codec;

// -------------------------------   DECODERS   ------------------------------------------

static csm_acse_code acse_proto_version_decoder(csm_asso_state *state, csm_ber *ber, csm_array *array)
{
    csm_acse_code ret = CSM_ACSE_ERR;
    (void) state;

    CSM_LOG("[ACSE] Found Protocol version tag");

    // we support only version1 of the protocol
    if (ber->length.length == 2U)
    {
        uint8_t version, unused_bytes;
        if (csm_array_read_u8(array, &unused_bytes))
        {
            if (csm_array_read_u8(array, &version))
            {
                if ((unused_bytes == 7U) && (version == 0x80U))
                {
                    ret = CSM_ACSE_OK;
                }
            }
        }
    }

    return ret;
}

/* Ref: See ISO/IEC 8650-1:11996 / ITU-T Rec. X.227 clause 7.1.4.1:

     7.1.4.1   Protocol Version
           For the requesting ACPM: The value assigned to  this  field  is  determined
     within the implementation of the ACPM. It is a variable length bit  string  where
     each bit that is set to one indicates the version of ACSE protocol that this ACPM
     supports. Bit 0 represents version 1; bit 1 represents version 2; etc..  Multiple
     bits may be set indicating support of multiple versions. No trailing bits  higher
     than the highest  version  of  this  Recommendation  which  the  requesting  ACPM
     supports are included. That is, the last bit of the string is set to one.
           For the accepting ACPM: The ACPM ignores trailing bits of this field  which
     are higher than the one indicating the  latest  version  of  this  Recommendation
     which it supports.
*/


static csm_acse_code acse_app_context_decoder(csm_asso_state *state, csm_ber *ber, csm_array *array)
{
    csm_acse_code ret = CSM_ACSE_ERR;
    (void) state;
    (void) array;

    CSM_LOG("[ACSE] Found APPLICATION CONTEXT tag");

    // the length of the object identifier must be 7 bytes + 2 bytes for the BER header = 9 bytes
    if (ber->length.length == 9U)
    {
        ret = CSM_ACSE_OK;
    }
    else
    {
        CSM_ERR("[ACSE] Bad object identifier size");
    }

    return ret;
}

/* Green Book 8

9.4.2.2       Registered COSEM names

    Within an OSI  environment, many different types  of network objects must be  identified  with globally
    unambiguous   names.   These   network   objects   include   abstract   syntaxes,   transfer   syntaxes,
    application  contexts,  authentication  mechanism  names,  etc.  Names  for  these  objects  in  most  cases
    are  assigned  by  the  committee  developing  the  particular  basic  ISO  standard  or  by  implementers’
    workshops,  and  should  be  registered.  For  DLMS/COSEM,  these  object  names  are  assigned  by  the
    DLMS UA, and are specified below.

    The  decision  no.  1999.01846  of  OFCOM,  Switzerland,  attributes  the  following  prefix  for  object
    identifiers specified by the DLMS User Association.

    { joint-iso-ccitt(2) country(16) country-name(756) identified-organisation(5) DLMS-UA(8) }

    For DLMS/COSEM, object identifiers are specified for naming the following items:
        --> COSEM application context names;
        --> COSEM authentication mechanism names;
        --> cryptographic algorithm ID-s.
*/
static csm_acse_code acse_oid_decoder(csm_asso_state *state, csm_ber *ber, csm_array *array)
{
    csm_acse_code ret = CSM_ACSE_ERR;
    (void) ber;

    CSM_LOG("[ACSE] Found OBJECT IDENTIFIER tag");

    ber_object_identifier oid;
    oid.header = cOidHeader;
    oid.size = 5U;

    if (csm_ber_decode_object_identifier(&oid, array) == TRUE)
    {
        // in the case of LN referencing, with no ciphering: 2, 16, 756, 5, 8, 1, 1;
        // in the case of SN referencing, with no ciphering: 2, 16, 756, 5, 8, 1, 2;

        if ((oid.name == APP_CONTEXT_NAME))
        {
            switch (oid.id)
            {
            case LN_REF:
                state->ref = LN_REF;
                ret = CSM_ACSE_OK;
                CSM_LOG("[ACSE] LogicalName referencing");
                break;
            case SN_REF:
                state->ref = SN_REF;
                ret = CSM_ACSE_OK;
                CSM_LOG("[ACSE] ShortName referencing");
                break;
            case LN_REF_WITH_CYPHERING:
                state->ref = LN_REF_WITH_CYPHERING;
                ret = CSM_ACSE_OK;
                CSM_LOG("[ACSE] LogicalName referencing with cyphering");
                break;

            case SN_REF_WITH_CYPHERING:
                state->ref = SN_REF_WITH_CYPHERING;
                ret = CSM_ACSE_OK;
                CSM_LOG("[ACSE] ShortName referencing with cyphering");
                break;

            default:
                CSM_LOG("[ACSE] Referencing not supported");
                break;
            }
        }
        // in the case of low-level-security: 2, 16, 756, 5, 8, 2, 1;
        // in the case of high-level-security (5): 2, 16, 756, 5, 8, 2, 5;
        else if ((oid.name == SECURITY_MECHANISM_NAME))
        {
            switch (oid.id)
            {
                case CSM_AUTH_LOW_LEVEL:
                    state->auth_level = CSM_AUTH_LOW_LEVEL;
                    ret = CSM_ACSE_OK;
                    CSM_LOG("[ACSE] Low level authentication");
                    break;
                case CSM_AUTH_HIGH_LEVEL_GMAC:
                    state->auth_level = CSM_AUTH_HIGH_LEVEL_GMAC;
                    ret = CSM_ACSE_OK;
                    CSM_LOG("[ACSE] High level authentication");
                    break;
                default:
                    CSM_LOG("[ACSE] Authentication level not supported");
                    break;
            }
        }
    }
    else
    {
        CSM_ERR("[ACSE] Bad Object Identifier contents or size");
    }

    return ret;
}


static csm_acse_code acse_req_decoder(csm_asso_state *state, csm_ber *ber, csm_array *array)
{
    csm_acse_code ret = CSM_ACSE_ERR;
    (void) state;

    CSM_LOG("[ACSE] Found sender requirements tag");

    if (ber->length.length == 2U)
    {
        // encoding of the authentication functional unit (0)
        // NOTE The number of bits coded may vary from client to client, but
        // within the COSEM environment, only bit 0 set to 1 (indicating the
        // requirement of the authentication functional unit) is to be respected.
        uint8_t byte;
        if (csm_array_read_u8(array, &byte))
        {
            // encoding of the number of unused bits in the last byte of the BIT STRING
            if (byte == 0x07U)
            {
                if (csm_array_read_u8(array, &byte))
                {
                    if (byte == 0x80U)
                    {
                        ret = CSM_ACSE_OK;
                    }
                }
            }
        }
    }
    else
    {
        CSM_ERR("[ACSE] Sender requirements bad size");
    }

    return ret;
}

static csm_acse_code acse_auth_value_decoder(csm_asso_state *state, csm_ber *ber, csm_array *array)
{
    csm_acse_code ret = CSM_ACSE_ERR;

    CSM_LOG("[ACSE] Found authentication value tag");
    if (csm_ber_decode(ber, array))
    {
        // Can be a challenge or a LLS
        if ((ber->length.length >= (CSM_DEF_LLS_SIZE)) &&
            (ber->length.length <= (CSM_DEF_CHALLENGE_SIZE)))
        {
            if (ber->tag.tag == (TAG_CONTEXT_SPECIFIC))
            {
                // It is a GraphicString, the size is dynamic
                if (csm_array_read_buff(array, &state->handshake.ctos.value[0], ber->length.length))
                {
                    state->handshake.ctos.size = ber->length.length;
                    ret = CSM_ACSE_OK;
                }
            }
        }

        if (ret == CSM_ACSE_ERR)
        {
            CSM_ERR("[ACSE] Bad authentication value size");
        }
    }
    else
    {
        CSM_ERR("[ACSE] Bad authentication value format");
    }

    return ret;
}


static csm_acse_code acse_user_info_decoder(csm_asso_state *state, csm_ber *ber, csm_array *array)
{
    csm_acse_code ret = CSM_ACSE_ERR;

    CSM_LOG("[ACSE] Found user info tag");
    if (csm_ber_decode(ber, array))
    {
        if (ber->tag.id == BER_TYPE_OCTET_STRING)
        {
            // Now decode the A-XDR encoded packet
            uint8_t byte;
            if(csm_array_read_u8(array, &byte))
            {
                if (byte == AXDR_INITIATE_REQUEST)
                {

                    /*
                    -- xDLMS APDU-s used during Association establishment
                    InitiateRequest ::= SEQUENCE
                    {
                    --  shall not be encoded in DLMS without ciphering
                    dedicated-key                      OCTET STRING OPTIONAL,
                    response-allowed                   BOOLEAN DEFAULT TRUE,
                    proposed-quality-of-service        [0] IMPLICIT Integer8 OPTIONAL,
                    proposed-dlms-version-number       Unsigned8,
                    proposed-conformance               Conformance, -- Shall be encoded in BER
                    client-max-receive-pdu-size        Unsigned16
                    }

                    -- The Conformance field shall be encoded in BER. See IEC 61334-6 Example 1.

                    */

                    CSM_LOG("[ACSE] Found xDLMS InitiateRequest encoded APDU");
                    if(csm_array_read_u8(array, &byte))
                    {
                        if (byte != AXDR_TAG_NULL)
                        {
                            // FIXME: copy the dedicated key
                        }
                    }

                    int valid = csm_axdr_rd_null(array); //  response-allowed
                    valid =  valid && csm_axdr_rd_null(array); // proposed_quality_of_service

                    // proposed-dlms-version-number: always 6
                    valid = valid && csm_array_read_u8(array, &byte);
                    valid = valid && (byte == 6U ? TRUE : FALSE);

                    // conformance, [APPLICATION 31] IMPLICIT BIT STRING
                    // encoding of the [APPLICATION 31] tag (ASN.1 explicit tag)
                    valid = valid && csm_ber_decode(ber, array);
                    if ((ber->tag.tag == 0x5FU) && (ber->tag.ext == 31U))
                    {
                        if (ber->length.length == 4U)
                        {
                            valid = valid && csm_array_read_u8(array, &byte);
                            valid = valid && (byte == 0U ? TRUE : FALSE); // unused bits in the bitstring

                            state->handshake.proposed_conformance = ((uint32_t)byte) << 16U;
                            valid = valid && csm_array_read_u8(array, &byte);
                            state->handshake.proposed_conformance += ((uint32_t)byte) << 8U;
                            valid = valid && csm_array_read_u8(array, &byte);
                            state->handshake.proposed_conformance += ((uint32_t)byte);

                            valid = valid && csm_array_read_u16(array, &state->handshake.client_max_receive_pdu_size);
                        }
                    }
                    else
                    {
                        valid = 0U;
                    }

                    if (valid)
                    {
                        ret = CSM_ACSE_OK;
                    }
                }
            }
        }
    }

    return ret;
}

static csm_acse_code acse_client_system_title_decoder(csm_asso_state *state, csm_ber *ber, csm_array *array)
{
    csm_acse_code ret = CSM_ACSE_ERR;

    CSM_LOG("[ACSE] Found client AP-Title tag");
    if (csm_ber_decode(ber, array))
    {
        // Can be a challenge or a LLS
        if (ber->length.length == CSM_DEF_APP_TITLE_SIZE)
        {
            if (ber->tag.id == BER_TYPE_OCTET_STRING)
            {
                // Store the AP-Title in the association context
                if (csm_array_read_buff(array, state->client_app_title, CSM_DEF_APP_TITLE_SIZE))
                {
                    ret = CSM_ACSE_OK;
                }
            }
        }

        if (ret == CSM_ACSE_ERR)
        {
            CSM_ERR("[ACSE] Bad AP-Title size");
        }
    }
    else
    {
        CSM_ERR("[ACSE] Bad AP-Title format");
    }

    return ret;
}


static csm_acse_code acse_skip_decoder(csm_asso_state *state, csm_ber *ber, csm_array *array)
{
    (void) state;
    if (ber->tag.isPrimitive)
    {
        // This BER contains data that is not managed
        // advance the pointer to the next BER header
        (void) csm_array_reader_jump(array, ber->length.length);
    }
    CSM_LOG("[ACSE] Skipped tag: %d", ber->tag.tag);
    return CSM_ACSE_OK;
}


/*
AARQ ::= [APPLICATION 0] IMPLICIT SEQUENCE
{
-- [APPLICATION 0] == [ 60H ] = [ 96 ]
protocol-version                   [0] IMPLICIT        BIT STRING {version1 (0)} DEFAULT {version1},
application-context-name           [1]                 Application-context-name,
called-AP-title                    [2]                 AP-title OPTIONAL,
called-AE-qualifier                [3]                 AE-qualifier OPTIONAL,
called-AP-invocation-id            [4]                 AP-invocation-identifier OPTIONAL,
called-AE-invocation-id            [5]                 AE-invocation-identifier OPTIONAL,
calling-AP-title                   [6]                 AP-title OPTIONAL,
calling-AE-qualifier               [7]                 AE-qualifier OPTIONAL,
calling-AP-invocation-id           [8]                 AP-invocation-identifier OPTIONAL,
calling-AE-invocation-id           [9]                 AE-invocation-identifier OPTIONAL,
-- The following field shall not be present if only the kernel is used.
sender-acse-requirements           [10] IMPLICIT      ACSE-requirements OPTIONAL,
-- The following field shall only be present if the authentication functional unit is selected.
mechanism-name                     [11] IMPLICIT      Mechanism-name OPTIONAL,
-- The following field shall only be present if the authentication functional unit is selected.
calling-authentication-value       [12] EXPLICIT      Authentication-value OPTIONAL,
implementation-information         [29] IMPLICIT      Implementation-data OPTIONAL,
user-information                   [30] EXPLICIT      Association-information OPTIONAL
}

-- The user-information field shall carry an InitiateRequest APDU encoded in A-XDR, and then
-- encoding the resulting OCTET STRING in BER.

*/
static const csm_asso_codec aarq_codec_chain[] =
{
    {CSM_ASSO_PROTO_VER,            ACSE_NONE, acse_proto_version_decoder, NULL},
    {CSM_ASSO_APP_CONTEXT_NAME,     ACSE_ANY, acse_app_context_decoder, NULL},
    {BER_TYPE_OBJECT_IDENTIFIER,    ACSE_ANY, acse_oid_decoder, NULL},
    {CSM_ASSO_CALLED_AP_TITLE,      ACSE_NONE, acse_skip_decoder, NULL},
    {CSM_ASSO_CALLED_AE_QUALIFIER,  ACSE_NONE, acse_skip_decoder, NULL},
    {CSM_ASSO_CALLED_AP_INVOC_ID,   ACSE_NONE, acse_skip_decoder, NULL},
    {BER_TYPE_INTEGER,              ACSE_NONE, acse_skip_decoder, NULL},
    {CSM_ASSO_CALLED_AE_INVOC_ID,   ACSE_NONE, acse_skip_decoder, NULL},
    {BER_TYPE_INTEGER,              ACSE_NONE, acse_skip_decoder, NULL},
    {CSM_ASSO_CALLING_AP_TITLE,     ACSE_OPT, acse_client_system_title_decoder, NULL},
    {CSM_ASSO_CALLING_AE_QUALIFIER, ACSE_NONE, acse_skip_decoder, NULL},
    {CSM_ASSO_CALLING_AP_INVOC_ID,  ACSE_NONE, acse_skip_decoder, NULL},
    {BER_TYPE_INTEGER,              ACSE_NONE, acse_skip_decoder, NULL},
    {CSM_ASSO_CALLING_AE_INVOC_ID,  ACSE_NONE, acse_skip_decoder, NULL},
    {BER_TYPE_INTEGER,              ACSE_NONE, acse_skip_decoder, NULL},
    {CSM_ASSO_SENDER_ACSE_REQU,     ACSE_OPT, acse_req_decoder, NULL},
    {CSM_ASSO_REQ_MECHANISM_NAME,   ACSE_OPT, acse_oid_decoder, NULL},
    {CSM_ASSO_CALLING_AUTH_VALUE,   ACSE_OPT, acse_auth_value_decoder, NULL},
    {CSM_ASSO_IMPLEMENTATION_INFO,  ACSE_OPT, acse_skip_decoder, NULL},
    {CSM_ASSO_USER_INFORMATION,     ACSE_OPT, acse_user_info_decoder, NULL}
};



#define CSM_ACSE_AARQ_CHAIN_SIZE   (sizeof(aarq_codec_chain)/sizeof(csm_asso_codec))


// -------------------------------   ENCODERS ------------------------------------------

static csm_acse_code acse_proto_version_encoder(csm_asso_state *state, csm_ber *ber, csm_array *array)
{
    csm_acse_code code = CSM_ACSE_ERR;
    (void) ber;
    (void) state;

    CSM_LOG("[ACSE] Encoding AARE ...");
    int ret = csm_ber_write_len(array, 2U);
    ret = ret && csm_array_write_u8(array, 7U); // unused bytes in the bit string
    ret = ret && csm_array_write_u8(array, 0x80U); // version1

    if (ret)
    {
        code = CSM_ACSE_OK;
    }
    return code;
}

static csm_acse_code acse_app_context_encoder(csm_asso_state *state, csm_ber *ber, csm_array *array)
{
    csm_acse_code ret = CSM_ACSE_ERR;
    (void) state;
    (void) ber;

    CSM_LOG("[ACSE] Encoding APPLICATION CONTEXT tag ...");

    // the length of the object identifier must be 7 bytes + 2 bytes for the BER header = 9 bytes
    if (csm_ber_write_len(array, 9U))
    {
        ret = CSM_ACSE_OK;
    }
    return ret;
}

static int acse_oid_encoder(csm_array *array, uint8_t name, uint8_t id)
{
    // the length of the object identifier must be 7 bytes
    int valid = csm_ber_write_len(array, 7U);
    valid = valid && csm_array_write_buff(array, &cOidHeader[0], 5U);
    valid = valid && csm_array_write_u8(array, name);
    valid = valid && csm_array_write_u8(array, id);
    return valid;
}


static csm_acse_code acse_oid_context_encoder(csm_asso_state *state, csm_ber *ber, csm_array *array)
{
    csm_acse_code ret = CSM_ACSE_ERR;
    (void) ber;

    CSM_LOG("[ACSE] Encoding Object Identifier tag ...");

    if (acse_oid_encoder(array, (uint8_t)APP_CONTEXT_NAME, (uint8_t)state->ref))
    {
        ret = CSM_ACSE_OK;
    }
    return ret;
}

/*
Association-result ::=                 INTEGER
{
    accepted                           (0),
    rejected-permanent                 (1),
    rejected-transient                 (2)
}
*/
static csm_acse_code acse_result_encoder(csm_asso_state *state, csm_ber *ber, csm_array *array)
{
    csm_acse_code ret = CSM_ACSE_ERR;
    (void) ber;

    CSM_LOG("[ACSE] Encoding result tag ...");

    uint8_t result = 0U; // accepted
    if (state->state_cf == CF_IDLE)
    {
        result = 1U; // rejected-permanent
    }

    if (csm_ber_write_integer(array, result))
    {
        ret = CSM_ACSE_OK;
    }

    return ret;
}


/*
Associate-source-diagnostic ::= CHOICE
{
    acse-service-user                  [1] INTEGER
    {
        null                                             (0),
        no-reason-given                                  (1),
        application-context-name-not-supported           (2),
        calling-AP-title-not-recognized                  (3),
        calling-AP-invocation-identifier-not-recognized  (4),
        calling-AE-qualifier-not-recognized              (5),
        calling-AE-invocation-identifier-not-recognized  (6),
        called-AP-title-not-recognized                   (7),
        called-AP-invocation-identifier-not-recognized   (8),
        called-AE-qualifier-not-recognized               (9),
        called-AE-invocation-identifier-not-recognized   (10),
        authentication-mechanism-name-not-recognised     (11),
        authentication-mechanism-name-required           (12),
        authentication-failure                           (13),
        authentication-required                          (14)
    },
    acse-service-provider              [2] INTEGER
    {
        null                               (0),
        no-reason-given                    (1),
        no-common-acse-version             (2)
    }
}
*/
static csm_acse_code acse_result_src_diag_encoder(csm_asso_state *state, csm_ber *ber, csm_array *array)
{
    csm_acse_code ret = CSM_ACSE_ERR;
    (void) ber;

    CSM_LOG("[ACSE] Encoding result source diagnostic tag ...");

    if (csm_ber_write_len(array, 5U))
    {
        if (csm_array_write_u8(array, (uint8_t)CSM_ASSO_RESULT_SERVICE_USER))
        {
            if (csm_ber_write_integer(array, state->handshake.result))
            {
                ret = CSM_ACSE_OK;
            }
        }
    }
    return ret;
}

static csm_acse_code acse_resp_system_title_encoder(csm_asso_state *state, csm_ber *ber, csm_array *array)
{
    csm_acse_code ret = CSM_ACSE_ERR;
    (void) state;
    (void) ber;

    CSM_LOG("[ACSE] Encoding server AP-Title ...");

    int valid = csm_ber_write_len(array, CSM_DEF_APP_TITLE_SIZE + 2U);
    valid = valid && csm_array_write_u8(array, BER_TYPE_OCTET_STRING);
    valid = valid && csm_array_write_u8(array, CSM_DEF_APP_TITLE_SIZE);
    valid = valid && csm_array_write_buff(array, csm_sys_get_system_title(), CSM_DEF_APP_TITLE_SIZE);

    if (valid)
    {
        ret = CSM_ACSE_OK;
    }
    return ret;
}

static csm_acse_code acse_responder_requirements_encoder(csm_asso_state *state, csm_ber *ber, csm_array *array)
{
    csm_acse_code ret = CSM_ACSE_ERR;
    (void) ber;
    (void) state;

    CSM_LOG("[ACSE] Encoding Responder ACSE requirements tag ...");

    int valid = csm_ber_write_len(array, 2U);
    valid = valid && csm_array_write_u8(array, 7U); // unused bits in the bit-string
    valid = valid && csm_array_write_u8(array, 0x80U);

    if (valid)
    {
        ret = CSM_ACSE_OK;
    }

    return ret;
}

static csm_acse_code acse_oid_mechanism_encoder(csm_asso_state *state, csm_ber *ber, csm_array *array)
{
    csm_acse_code ret = CSM_ACSE_ERR;
    (void) ber;

    CSM_LOG("[ACSE] Encoding Object Identifier tag ...");

    if (acse_oid_encoder(array, (uint8_t)SECURITY_MECHANISM_NAME, (uint8_t)state->auth_level))
    {
        ret = CSM_ACSE_OK;
    }
    return ret;
}

#ifdef GB_TEST_VECTORS
char stoc[] = "P6wRJ21F";
#endif

static csm_acse_code acse_responder_auth_value_encoder(csm_asso_state *state, csm_ber *ber, csm_array *array)
{
    csm_acse_code ret = CSM_ACSE_ERR;
    (void) ber;

    CSM_LOG("[ACSE] Encoding Responder authentication value ...");

    // Generate the same challenge size than the client
    // FIXME: randomize the size for the StoC challenge?
    uint8_t size = state->handshake.ctos.size;
    state->handshake.stoc.size = size;

    int valid = csm_ber_write_len(array, size + 2U);
    valid = valid && csm_array_write_u8(array, TAG_CONTEXT_SPECIFIC); // GraphicsString

    // Serialize the server authentication value to the output buffer and in our scratch buffer
    valid = valid && csm_array_write_u8(array, size);
    for (uint8_t i = 0U; i < size; i++)
    {
#ifdef GB_TEST_VECTORS
        uint8_t byte = stoc[i];
#else
        uint8_t byte = csm_sys_get_random_u8();
#endif

        valid = valid && csm_array_write_u8(array, byte);
        state->handshake.stoc.value[i] = byte;
    }

    if (valid)
    {
        ret = CSM_ACSE_OK;
    }

    return ret;
}

static csm_acse_code acse_user_info_encoder(csm_asso_state *state, csm_ber *ber, csm_array *array)
{
    csm_acse_code ret = CSM_ACSE_ERR;
    (void) ber;

    CSM_LOG("[ACSE] Encoding user info tag ...");

  /*
    InitiateResponse ::= SEQUENCE
    {
    negotiated-quality-of-service      [0] IMPLICIT Integer8 OPTIONAL,
    negotiated-dlms-version-number     Unsigned8,
    negotiated-conformance             Conformance, -- Shall be encoded in BER
    server-max-receive-pdu-size        Unsigned16,
    vaa-name                           ObjectName
    }
    -- In the case of LN referencing, the value of the vaa-name is 0x0007
    -- In the case of SN referencing, the value of the vaa-name is the base name of the
    -- Current Association object, 0xFA00
   */


    uint32_t saved_index = array->wr_index; // save the location of the UserInfo structure size

    int valid = csm_array_write_u8(array, 0U); // size of the structure, will be updated at the end
    valid = valid && csm_array_write_u8(array, BER_TYPE_OCTET_STRING);
    valid = valid && csm_array_write_u8(array, 0U); // size of the octet-string, will be updated at the end

    // Now encode the A-XDR encoded packet
    valid = valid && csm_array_write_u8(array, AXDR_INITIATE_RESPONSE);
    valid = valid && csm_array_write_u8(array, 0U); // null, no QoS
    valid = valid && csm_array_write_u8(array, 6U);// negotiated-dlms-version-number

    // Conformance block   FIXME: to be clean, rely on a real BER encoder for the long TAG
    valid = valid && csm_array_write_u8(array, 0x5FU);
    valid = valid && csm_array_write_u8(array, 0x1FU);
    valid = valid && csm_array_write_u8(array, 4U); // Size of the conformance block data
    valid = valid && csm_array_write_u8(array, 0U); // unused bits in the bit-string

    // Serialize the conformance block (3 bytes)
    uint8_t byte = (state->config->conformance >> 16U) & 0xFFU;
    valid = valid && csm_array_write_u8(array, byte);
    byte = (state->config->conformance >> 8U) & 0xFFU;
    valid = valid && csm_array_write_u8(array, byte);
    byte = state->config->conformance & 0xFFU;
    valid = valid && csm_array_write_u8(array, byte);

    // server-max-receive-pdu-size
    byte = (CSM_DEF_PDU_SIZE >> 8U) & 0xFFU;
    valid = valid && csm_array_write_u8(array, byte);
    byte = CSM_DEF_PDU_SIZE & 0xFFU;
    valid = valid && csm_array_write_u8(array, byte);

    if ((state->ref == LN_REF) || (state->ref == LN_REF_WITH_CYPHERING))
    {
        valid = valid && csm_array_write_u8(array, 0U);
        valid = valid && csm_array_write_u8(array, 7U);
    }
    else
    {
        valid = valid && csm_array_write_u8(array, 0xFAU);
        valid = valid && csm_array_write_u8(array, 0U);
    }

    // Update the size of the initiate response elements
    uint32_t size = array->wr_index - saved_index;

    valid = valid && csm_array_set(array, saved_index, size - 1U); // minus the size byte field
    valid = valid && csm_array_set(array, saved_index + 2U, size - 3U); // minus whole header (BER size + Octet-String tag + length)

    if (valid)
    {
        ret = CSM_ACSE_OK;
    }

    return ret;
}

/**

AARE-apdu ::= [APPLICATION 1] IMPLICIT SEQUENCE
{
-- [APPLICATION 1] == [ 61H ] = [ 97 ]
protocol-version                     [0] IMPLICIT BIT STRING {version1 (0)} DEFAULT
{version1},
application-context-name             [1]             Application-context-name,
result                               [2]             Association-result,
result-source-diagnostic             [3]             Associate-source-diagnostic,
responding-AP-title                  [4]             AP-title OPTIONAL,
responding-AE-qualifier              [5]             AE-qualifier OPTIONAL,
responding-AP-invocation-id          [6]             AP-invocation-identifier OPTIONAL,
responding-AE-invocation-id          [7]             AE-invocation-identifier OPTIONAL,
-- The following field shall not be present if only the kernel is used.
responder-acse-requirements          [8] IMPLICIT    ACSE-requirements OPTIONAL,
--  The  following  field  shall  only  be  present  if  the  authentication  functional  unit  is selected.
mechanism-name                       [9] IMPLICIT    Mechanism-name OPTIONAL,
--  The  following  field  shall  only  be  present  if  the  authentication  functional  unit  is selected.
responding-authentication-value      [10] EXPLICIT   Authentication-value OPTIONAL,
implementation-information           [29] IMPLICIT   Implementation-data OPTIONAL,
user-information                     [30] EXPLICIT   Association-information OPTIONAL
}
-- The user-information field shall carry either an InitiateResponse (or, when the proposed
-- xDLMS  context  is not  accepted  by  the  server,  a  confirmedServiceError  APDU  encoded  in
-- A-XDR, and then encoding the resulting OCTET STRING in BER.

  */

// FIXME: export context field in the configuration file
static const csm_asso_codec aare_codec_chain[] =
{
    {CSM_ASSO_PROTO_VER,            ACSE_NONE, NULL, acse_proto_version_encoder},
    {CSM_ASSO_APP_CONTEXT_NAME,     ACSE_ANY, NULL, acse_app_context_encoder},
    {BER_TYPE_OBJECT_IDENTIFIER,    ACSE_ANY, NULL, acse_oid_context_encoder},
    {CSM_ASSO_RESULT_FIELD,         ACSE_ANY, NULL, acse_result_encoder},
    {CSM_ASSO_RESULT_SRC_DIAG,      ACSE_ANY, NULL, acse_result_src_diag_encoder},

    // Additional fields specific when cyphered authentication is required
    {CSM_ASSO_RESP_AP_TITLE,        ACSE_SEC, NULL, acse_resp_system_title_encoder},
    {CSM_ASSO_RESPONDER_ACSE_REQ,   ACSE_SEC, NULL, acse_responder_requirements_encoder},
    {CSM_ASSO_RESP_MECHANISM_NAME,  ACSE_SEC, NULL, acse_oid_mechanism_encoder},
    {CSM_ASSO_RESP_AUTH_VALUE,      ACSE_SEC, NULL, acse_responder_auth_value_encoder},

    // Final field
    {CSM_ASSO_USER_INFORMATION,     ACSE_ANY, NULL, acse_user_info_encoder},

};

#define CSM_ACSE_AARE_CHAIN_SIZE   (sizeof(aare_codec_chain)/sizeof(csm_asso_codec))

/*
RLRQ-apdu ::= [APPLICATION 2] IMPLICIT SEQUENCE
{
-- [APPLICATION 2] == [ 62H ] = [ 98 ]
reason                             [0] IMPLICIT        Release-request-reason OPTIONAL,
user-information  [30] EXPLICIT       Association-information OPTIONAL
}
RLRE-apdu ::= [APPLICATION 3] IMPLICIT SEQUENCE
{
-- [APPLICATION 3] == [ 63H ] = [ 99 ]
reason                             [0] IMPLICIT        Release-response-reason OPTIONAL,
user-information                   [30] EXPLICIT       Association-information OPTIONAL
}
-- The user-information field of the RLRQ / RLRE APDU may carry an InitiateRequest APDU encoded in
-- A-XDR, and then encoding the resulting OCTET STRING in BER, when the AA to be released uses
-- ciphering.

*/


// --------------------------  ASSOCIATION MAIN FUNCTIONS -------------------------------------------

void csm_asso_init(csm_asso_state *state)
{
    state->state_cf = CF_IDLE;
    state->auth_level = CSM_AUTH_LOWEST_LEVEL;
    state->ref = NO_REF;
    state->handshake.result = CSM_ASSO_ERR_NULL;
}

// Check is association is granted
int csm_asso_is_granted(csm_asso_state *state)
{
    int ret = FALSE;
    if (state->state_cf == CF_IDLE)
    {
        // Test the password if required
        if (state->auth_level == CSM_AUTH_LOWEST_LEVEL)
        {
            state->state_cf = CF_ASSOCIATED;
            state->handshake.result = CSM_ASSO_ERR_NULL;
            ret = TRUE;
        }
        else if (state->auth_level == CSM_AUTH_LOW_LEVEL)
        {
            if (csm_sys_test_lls_password(state->config->llc.dsap, &state->handshake.ctos.value[0], state->handshake.ctos.size))
            {
                state->state_cf = CF_ASSOCIATED;
                state->handshake.result = CSM_ASSO_ERR_NULL;
                ret = TRUE;
            }
            else
            {
                state->handshake.result = CSM_ASSO_ERR_AUTH_FAILURE;
            }
        }
        else if (state->auth_level == CSM_AUTH_HIGH_LEVEL_GMAC)
        {
            state->state_cf = CF_ASSOCIATION_PENDING;
            state->handshake.result = CSM_ASSO_AUTH_REQUIRED;
            ret = TRUE;
        }
        else
        {
            // Failure, other cases are not managed
            CSM_ERR("[ACSE] Access refused, bad authentication level");
            state->handshake.result = CSM_ASSO_AUTH_UNKNOWN;
        }
    }

    return ret;
}

int csm_asso_decoder(csm_asso_state *state, csm_array *array)
{
    csm_ber ber;
    uint8_t decoder_index = 0U;

    // Decode first bytes
    int ret = csm_ber_decode(&ber, array);
    if ((ber.length.length != csm_array_unread(array)) ||
        (ber.tag.tag != CSM_ASSO_AARQ))
    {
        CSM_ERR("[ACSE] Bad AARQ size");
        ret = CSM_ACSE_ERR;
    }
    else
    {
        // Main decoding loop
        ret = csm_ber_decode(&ber, array);
        do
        {
            if (ret)
            {
                const csm_asso_codec *codec = &aarq_codec_chain[decoder_index];
                decoder_index++;
                if (ber.tag.tag == codec->tag)
                {
                    ret = FALSE;
                    if ((codec->extract_func != NULL))
                    {
                        ret = codec->extract_func(state, &ber, array);
                    }

                    if ((codec->context == ACSE_OPT) && !ret)
                    {
                        ret = TRUE; // normal error (optional field)
                    }

                    if ((ret) && (decoder_index < CSM_ACSE_AARQ_CHAIN_SIZE))
                    {
                        // Continue decoding BER
                        ret = csm_ber_decode(&ber, array);
                    }
                }
            }

            if (!ret)
            {
                break;
            }
        }
        while (decoder_index < CSM_ACSE_AARQ_CHAIN_SIZE);
    }

    return ret;
}

int csm_asso_encoder(csm_asso_state *state, csm_array *array)
{
    array->wr_index = 0U; // Reinit write pointer
    int ret = FALSE;
    csm_ber ber;

    if (csm_array_write_u8(array, CSM_ASSO_AARE))
    {
        // Write dummy size, it will be updated later
        // Since the AARE is never bigger than 127, the length encoding can one-byte size
        if (csm_array_write_u8(array, 0U))
        {
            const csm_asso_codec *codec = &aare_codec_chain[0];
            uint32_t i = 0U;
            for (i = 0U; i < CSM_ACSE_AARE_CHAIN_SIZE; i++)
            {
                // Don't encode optional data
                if ((codec[i].insert_func != NULL) && (codec[i].context != ACSE_NONE))
                {
                    // Don't encode some fields when no security is required
                    if ((state->auth_level != CSM_AUTH_HIGH_LEVEL_GMAC) && (codec[i].context == ACSE_SEC))
                    {
                        continue;
                    }
                    else
                    {
                        // Insert codec tag identifier
                        if (csm_array_write_u8(array, codec[i].tag))
                        {
                            if (!codec[i].insert_func(state, &ber, array))
                            {
                                // Exit on error
                                break;
                            }
                        }
                    }
                }
            }

            if (i >= CSM_ACSE_AARE_CHAIN_SIZE)
            {
                ret = TRUE;
                // Update the size
                csm_array_set(array, 1U, array->wr_index - 2U); // skip the BER header (tag+length = 2 bytes)
            }
            else
            {
                CSM_ERR("[ACSE] Encoding chain error");
            }
        }
    }
    return ret;
}

int csm_asso_execute(csm_asso_state *asso, csm_array *packet)
{
    int bytes_to_reply = 0;

    if (asso->state_cf  == CF_IDLE)
    {
        if (csm_asso_decoder(asso, packet))
        {
            if (csm_asso_is_granted(asso))
            {
                CSM_LOG("[ACSE] Access granted!");
            }
            else
            {
                // FIXME: print textual reason
                CSM_ERR("[ACSE] Connection rejected, reason: %d", asso->handshake.result);
            }

            // Send AARE, success or failure
            if (csm_asso_encoder(asso, packet))
            {
                bytes_to_reply = packet->wr_index;
                CSM_LOG("[ACSE] AARE length: %d", bytes_to_reply);
             //   csm_array_dump(packet);
            }
        }
        else
        {
            CSM_ERR("[ACSE] BER decoding error");
        }
    }
    else if (asso->state_cf  == CF_ASSOCIATED)
    {
        uint8_t byte;
        // Associated, so maybe it is an RLRQ disconnection packet
        if (csm_array_get(packet, 0U, &byte))
        {
            if (byte == CSM_ASSO_RLRQ)
            {
                CSM_LOG("[ACSE] RLRQ Received, send RLRE");
                asso->state_cf = CF_IDLE;
                packet->wr_index = 0U;
                // FIXME: for now, send minimal fixed raw RLRE reply
                static uint8_t rlre[] = { CSM_ASSO_RLRE, 3U, 0x80U, 0x01U, 0x00U };

                csm_array_write_buff(packet, rlre, 5U);
                bytes_to_reply = 5U;
            }
            else
            {
                CSM_ERR("[ACSE] Bad tag received: %X", byte);
            }
        }
    }

    return bytes_to_reply;
}
