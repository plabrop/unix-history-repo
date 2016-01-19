/*
 * Namespace munging inspired by an equivalent hack in NetBSD's tree: add
 * the "ssh_" prefix to every symbol in libssh which doesn't already have
 * it.  This prevents collisions between symbols in libssh and symbols in
 * other libraries or applications which link with libssh, either directly
 * or indirectly (e.g. through PAM loading pam_ssh).
 *
 * A list of symbols which need munging is obtained as follows:
 *
 # nm libprivatessh.a | LC_ALL=C awk '
     /^[0-9a-z]+ [Tt] [A-Za-z_][0-9A-Za-z_]*$/ && $3 !~ /^ssh_/ {
         printf("#define %-39s ssh_%s\n", $3, $3)
     }' | unexpand -a | LC_ALL=C sort -u
 *
 * $FreeBSD$
 */

#define Blowfish_decipher			ssh_Blowfish_decipher
#define Blowfish_encipher			ssh_Blowfish_encipher
#define Blowfish_expand0state			ssh_Blowfish_expand0state
#define Blowfish_expandstate			ssh_Blowfish_expandstate
#define Blowfish_initstate			ssh_Blowfish_initstate
#define Blowfish_stream2word			ssh_Blowfish_stream2word
#define a2port					ssh_a2port
#define a2tun					ssh_a2tun
#define add_host_to_hostfile			ssh_add_host_to_hostfile
#define add_p1p1				ssh_add_p1p1
#define addargs					ssh_addargs
#define addr_match_cidr_list			ssh_addr_match_cidr_list
#define addr_match_list				ssh_addr_match_list
#define addr_netmatch				ssh_addr_netmatch
#define addr_pton				ssh_addr_pton
#define addr_pton_cidr				ssh_addr_pton_cidr
#define ask_permission				ssh_ask_permission
#define atomicio				ssh_atomicio
#define atomicio6				ssh_atomicio6
#define atomiciov				ssh_atomiciov
#define atomiciov6				ssh_atomiciov6
#define auth_request_forwarding			ssh_auth_request_forwarding
#define bandwidth_limit				ssh_bandwidth_limit
#define bandwidth_limit_init			ssh_bandwidth_limit_init
#define barrett_reduce				ssh_barrett_reduce
#define bcrypt_hash				ssh_bcrypt_hash
#define bcrypt_pbkdf				ssh_bcrypt_pbkdf
#define bf_ssh1_cipher				ssh_bf_ssh1_cipher
#define blf_cbc_decrypt				ssh_blf_cbc_decrypt
#define blf_cbc_encrypt				ssh_blf_cbc_encrypt
#define blf_dec					ssh_blf_dec
#define blf_ecb_decrypt				ssh_blf_ecb_decrypt
#define blf_ecb_encrypt				ssh_blf_ecb_encrypt
#define blf_enc					ssh_blf_enc
#define blf_key					ssh_blf_key
#define buffer_append				ssh_buffer_append
#define buffer_append_space			ssh_buffer_append_space
#define buffer_check_alloc			ssh_buffer_check_alloc
#define buffer_compress				ssh_buffer_compress
#define buffer_compress_init_recv		ssh_buffer_compress_init_recv
#define buffer_compress_init_send		ssh_buffer_compress_init_send
#define buffer_compress_uninit			ssh_buffer_compress_uninit
#define buffer_consume				ssh_buffer_consume
#define buffer_consume_end			ssh_buffer_consume_end
#define buffer_consume_end_ret			ssh_buffer_consume_end_ret
#define buffer_consume_ret			ssh_buffer_consume_ret
#define buffer_get				ssh_buffer_get
#define buffer_get_bignum			ssh_buffer_get_bignum
#define buffer_get_bignum2			ssh_buffer_get_bignum2
#define buffer_get_bignum2_ret			ssh_buffer_get_bignum2_ret
#define buffer_get_bignum_ret			ssh_buffer_get_bignum_ret
#define buffer_get_char				ssh_buffer_get_char
#define buffer_get_char_ret			ssh_buffer_get_char_ret
#define buffer_get_cstring			ssh_buffer_get_cstring
#define buffer_get_cstring_ret			ssh_buffer_get_cstring_ret
#define buffer_get_ecpoint			ssh_buffer_get_ecpoint
#define buffer_get_ecpoint_ret			ssh_buffer_get_ecpoint_ret
#define buffer_get_int				ssh_buffer_get_int
#define buffer_get_int64			ssh_buffer_get_int64
#define buffer_get_int64_ret			ssh_buffer_get_int64_ret
#define buffer_get_int_ret			ssh_buffer_get_int_ret
#define buffer_get_ret				ssh_buffer_get_ret
#define buffer_get_short			ssh_buffer_get_short
#define buffer_get_short_ret			ssh_buffer_get_short_ret
#define buffer_get_string			ssh_buffer_get_string
#define buffer_get_string_ptr			ssh_buffer_get_string_ptr
#define buffer_get_string_ptr_ret		ssh_buffer_get_string_ptr_ret
#define buffer_get_string_ret			ssh_buffer_get_string_ret
#define buffer_put_bignum			ssh_buffer_put_bignum
#define buffer_put_bignum2			ssh_buffer_put_bignum2
#define buffer_put_bignum2_from_string		ssh_buffer_put_bignum2_from_string
#define buffer_put_bignum2_ret			ssh_buffer_put_bignum2_ret
#define buffer_put_bignum_ret			ssh_buffer_put_bignum_ret
#define buffer_put_char				ssh_buffer_put_char
#define buffer_put_cstring			ssh_buffer_put_cstring
#define buffer_put_ecpoint			ssh_buffer_put_ecpoint
#define buffer_put_ecpoint_ret			ssh_buffer_put_ecpoint_ret
#define buffer_put_int				ssh_buffer_put_int
#define buffer_put_int64			ssh_buffer_put_int64
#define buffer_put_short			ssh_buffer_put_short
#define buffer_put_string			ssh_buffer_put_string
#define buffer_uncompress			ssh_buffer_uncompress
#define cert_free				ssh_cert_free
#define cert_new				ssh_cert_new
#define chacha_encrypt_bytes			ssh_chacha_encrypt_bytes
#define chacha_ivsetup				ssh_chacha_ivsetup
#define chacha_keysetup				ssh_chacha_keysetup
#define chachapoly_crypt			ssh_chachapoly_crypt
#define chachapoly_get_length			ssh_chachapoly_get_length
#define chachapoly_init				ssh_chachapoly_init
#define chan_ibuf_empty				ssh_chan_ibuf_empty
#define chan_is_dead				ssh_chan_is_dead
#define chan_mark_dead				ssh_chan_mark_dead
#define chan_obuf_empty				ssh_chan_obuf_empty
#define chan_rcvd_eow				ssh_chan_rcvd_eow
#define chan_rcvd_ieof				ssh_chan_rcvd_ieof
#define chan_rcvd_oclose			ssh_chan_rcvd_oclose
#define chan_read_failed			ssh_chan_read_failed
#define chan_send_eof2				ssh_chan_send_eof2
#define chan_send_oclose1			ssh_chan_send_oclose1
#define chan_shutdown_read			ssh_chan_shutdown_read
#define chan_shutdown_write			ssh_chan_shutdown_write
#define chan_write_failed			ssh_chan_write_failed
#define channel_add_adm_permitted_opens		ssh_channel_add_adm_permitted_opens
#define channel_add_permitted_opens		ssh_channel_add_permitted_opens
#define channel_after_select			ssh_channel_after_select
#define channel_by_id				ssh_channel_by_id
#define channel_cancel_cleanup			ssh_channel_cancel_cleanup
#define channel_cancel_lport_listener		ssh_channel_cancel_lport_listener
#define channel_cancel_rport_listener		ssh_channel_cancel_rport_listener
#define channel_clear_adm_permitted_opens	ssh_channel_clear_adm_permitted_opens
#define channel_clear_permitted_opens		ssh_channel_clear_permitted_opens
#define channel_close_all			ssh_channel_close_all
#define channel_close_fd			ssh_channel_close_fd
#define channel_close_fds			ssh_channel_close_fds
#define channel_connect_by_listen_address	ssh_channel_connect_by_listen_address
#define channel_connect_by_listen_path		ssh_channel_connect_by_listen_path
#define channel_connect_stdio_fwd		ssh_channel_connect_stdio_fwd
#define channel_connect_to_path			ssh_channel_connect_to_path
#define channel_connect_to_port			ssh_channel_connect_to_port
#define channel_disable_adm_local_opens		ssh_channel_disable_adm_local_opens
#define channel_find_open			ssh_channel_find_open
#define channel_free				ssh_channel_free
#define channel_free_all			ssh_channel_free_all
#define channel_fwd_bind_addr			ssh_channel_fwd_bind_addr
#define channel_handler				ssh_channel_handler
#define channel_input_close			ssh_channel_input_close
#define channel_input_close_confirmation	ssh_channel_input_close_confirmation
#define channel_input_data			ssh_channel_input_data
#define channel_input_extended_data		ssh_channel_input_extended_data
#define channel_input_ieof			ssh_channel_input_ieof
#define channel_input_oclose			ssh_channel_input_oclose
#define channel_input_open_confirmation		ssh_channel_input_open_confirmation
#define channel_input_open_failure		ssh_channel_input_open_failure
#define channel_input_port_forward_request	ssh_channel_input_port_forward_request
#define channel_input_port_open			ssh_channel_input_port_open
#define channel_input_status_confirm		ssh_channel_input_status_confirm
#define channel_input_window_adjust		ssh_channel_input_window_adjust
#define channel_lookup				ssh_channel_lookup
#define channel_new				ssh_channel_new
#define channel_not_very_much_buffered_data	ssh_channel_not_very_much_buffered_data
#define channel_open_message			ssh_channel_open_message
#define channel_output_poll			ssh_channel_output_poll
#define channel_permit_all_opens		ssh_channel_permit_all_opens
#define channel_post_auth_listener		ssh_channel_post_auth_listener
#define channel_post_connecting			ssh_channel_post_connecting
#define channel_post_mux_client			ssh_channel_post_mux_client
#define channel_post_mux_listener		ssh_channel_post_mux_listener
#define channel_post_open			ssh_channel_post_open
#define channel_post_output_drain_13		ssh_channel_post_output_drain_13
#define channel_post_port_listener		ssh_channel_post_port_listener
#define channel_post_x11_listener		ssh_channel_post_x11_listener
#define channel_pre_connecting			ssh_channel_pre_connecting
#define channel_pre_dynamic			ssh_channel_pre_dynamic
#define channel_pre_input_draining		ssh_channel_pre_input_draining
#define channel_pre_listener			ssh_channel_pre_listener
#define channel_pre_mux_client			ssh_channel_pre_mux_client
#define channel_pre_open			ssh_channel_pre_open
#define channel_pre_open_13			ssh_channel_pre_open_13
#define channel_pre_output_draining		ssh_channel_pre_output_draining
#define channel_pre_x11_open			ssh_channel_pre_x11_open
#define channel_pre_x11_open_13			ssh_channel_pre_x11_open_13
#define channel_prepare_select			ssh_channel_prepare_select
#define channel_print_adm_permitted_opens	ssh_channel_print_adm_permitted_opens
#define channel_register_cleanup		ssh_channel_register_cleanup
#define channel_register_fds			ssh_channel_register_fds
#define channel_register_filter			ssh_channel_register_filter
#define channel_register_open_confirm		ssh_channel_register_open_confirm
#define channel_register_status_confirm		ssh_channel_register_status_confirm
#define channel_request_remote_forwarding	ssh_channel_request_remote_forwarding
#define channel_request_rforward_cancel		ssh_channel_request_rforward_cancel
#define channel_request_start			ssh_channel_request_start
#define channel_send_open			ssh_channel_send_open
#define channel_send_window_changes		ssh_channel_send_window_changes
#define channel_set_af				ssh_channel_set_af
#define channel_set_fds				ssh_channel_set_fds
#define channel_setup_fwd_listener_streamlocal	ssh_channel_setup_fwd_listener_streamlocal
#define channel_setup_fwd_listener_tcpip	ssh_channel_setup_fwd_listener_tcpip
#define channel_setup_local_fwd_listener	ssh_channel_setup_local_fwd_listener
#define channel_setup_remote_fwd_listener	ssh_channel_setup_remote_fwd_listener
#define channel_still_open			ssh_channel_still_open
#define channel_stop_listening			ssh_channel_stop_listening
#define channel_update_permitted_opens		ssh_channel_update_permitted_opens
#define check_crc				ssh_check_crc
#define check_hostkeys_by_key_or_type		ssh_check_hostkeys_by_key_or_type
#define check_key_in_hostkeys			ssh_check_key_in_hostkeys
#define choose_dh				ssh_choose_dh
#define choose_t				ssh_choose_t
#define chop					ssh_chop
#define cipher_alg_list				ssh_cipher_alg_list
#define cipher_authlen				ssh_cipher_authlen
#define cipher_blocksize			ssh_cipher_blocksize
#define cipher_by_name				ssh_cipher_by_name
#define cipher_by_number			ssh_cipher_by_number
#define cipher_cleanup				ssh_cipher_cleanup
#define cipher_crypt				ssh_cipher_crypt
#define cipher_get_keycontext			ssh_cipher_get_keycontext
#define cipher_get_keyiv			ssh_cipher_get_keyiv
#define cipher_get_keyiv_len			ssh_cipher_get_keyiv_len
#define cipher_get_length			ssh_cipher_get_length
#define cipher_get_number			ssh_cipher_get_number
#define cipher_init				ssh_cipher_init
#define cipher_is_cbc				ssh_cipher_is_cbc
#define cipher_ivlen				ssh_cipher_ivlen
#define cipher_keylen				ssh_cipher_keylen
#define cipher_mask_ssh1			ssh_cipher_mask_ssh1
#define cipher_name				ssh_cipher_name
#define cipher_number				ssh_cipher_number
#define cipher_seclen				ssh_cipher_seclen
#define cipher_set_key_string			ssh_cipher_set_key_string
#define cipher_set_keycontext			ssh_cipher_set_keycontext
#define cipher_set_keyiv			ssh_cipher_set_keyiv
#define cipher_warning_message			ssh_cipher_warning_message
#define ciphers_valid				ssh_ciphers_valid
#define cleanhostname				ssh_cleanhostname
#define cleanup_exit				ssh_cleanup_exit
#define clear_cached_addr			ssh_clear_cached_addr
#define colon					ssh_colon
#define compare					ssh_compare
#define compare_gps				ssh_compare_gps
#define compat_cipher_proposal			ssh_compat_cipher_proposal
#define compat_datafellows			ssh_compat_datafellows
#define compat_kex_proposal			ssh_compat_kex_proposal
#define compat_pkalg_proposal			ssh_compat_pkalg_proposal
#define connect_next				ssh_connect_next
#define connect_to				ssh_connect_to
#define convtime				ssh_convtime
#define crypto_hash_sha512			ssh_crypto_hash_sha512
#define crypto_hashblocks_sha512		ssh_crypto_hashblocks_sha512
#define crypto_scalarmult_curve25519		ssh_crypto_scalarmult_curve25519
#define crypto_sign_ed25519			ssh_crypto_sign_ed25519
#define crypto_sign_ed25519_keypair		ssh_crypto_sign_ed25519_keypair
#define crypto_sign_ed25519_open		ssh_crypto_sign_ed25519_open
#define crypto_sign_ed25519_ref_double_scalarmult_vartime ssh_crypto_sign_ed25519_ref_double_scalarmult_vartime
#define crypto_sign_ed25519_ref_fe25519_add	ssh_crypto_sign_ed25519_ref_fe25519_add
#define crypto_sign_ed25519_ref_fe25519_cmov	ssh_crypto_sign_ed25519_ref_fe25519_cmov
#define crypto_sign_ed25519_ref_fe25519_freeze	ssh_crypto_sign_ed25519_ref_fe25519_freeze
#define crypto_sign_ed25519_ref_fe25519_getparity ssh_crypto_sign_ed25519_ref_fe25519_getparity
#define crypto_sign_ed25519_ref_fe25519_invert	ssh_crypto_sign_ed25519_ref_fe25519_invert
#define crypto_sign_ed25519_ref_fe25519_iseq_vartime ssh_crypto_sign_ed25519_ref_fe25519_iseq_vartime
#define crypto_sign_ed25519_ref_fe25519_iszero	ssh_crypto_sign_ed25519_ref_fe25519_iszero
#define crypto_sign_ed25519_ref_fe25519_mul	ssh_crypto_sign_ed25519_ref_fe25519_mul
#define crypto_sign_ed25519_ref_fe25519_neg	ssh_crypto_sign_ed25519_ref_fe25519_neg
#define crypto_sign_ed25519_ref_fe25519_pack	ssh_crypto_sign_ed25519_ref_fe25519_pack
#define crypto_sign_ed25519_ref_fe25519_pow2523 ssh_crypto_sign_ed25519_ref_fe25519_pow2523
#define crypto_sign_ed25519_ref_fe25519_setone	ssh_crypto_sign_ed25519_ref_fe25519_setone
#define crypto_sign_ed25519_ref_fe25519_setzero ssh_crypto_sign_ed25519_ref_fe25519_setzero
#define crypto_sign_ed25519_ref_fe25519_square	ssh_crypto_sign_ed25519_ref_fe25519_square
#define crypto_sign_ed25519_ref_fe25519_sub	ssh_crypto_sign_ed25519_ref_fe25519_sub
#define crypto_sign_ed25519_ref_fe25519_unpack	ssh_crypto_sign_ed25519_ref_fe25519_unpack
#define crypto_sign_ed25519_ref_isneutral_vartime ssh_crypto_sign_ed25519_ref_isneutral_vartime
#define crypto_sign_ed25519_ref_pack		ssh_crypto_sign_ed25519_ref_pack
#define crypto_sign_ed25519_ref_sc25519_2interleave2 ssh_crypto_sign_ed25519_ref_sc25519_2interleave2
#define crypto_sign_ed25519_ref_sc25519_add	ssh_crypto_sign_ed25519_ref_sc25519_add
#define crypto_sign_ed25519_ref_sc25519_from32bytes ssh_crypto_sign_ed25519_ref_sc25519_from32bytes
#define crypto_sign_ed25519_ref_sc25519_from64bytes ssh_crypto_sign_ed25519_ref_sc25519_from64bytes
#define crypto_sign_ed25519_ref_sc25519_from_shortsc ssh_crypto_sign_ed25519_ref_sc25519_from_shortsc
#define crypto_sign_ed25519_ref_sc25519_isshort_vartime ssh_crypto_sign_ed25519_ref_sc25519_isshort_vartime
#define crypto_sign_ed25519_ref_sc25519_iszero_vartime ssh_crypto_sign_ed25519_ref_sc25519_iszero_vartime
#define crypto_sign_ed25519_ref_sc25519_lt_vartime ssh_crypto_sign_ed25519_ref_sc25519_lt_vartime
#define crypto_sign_ed25519_ref_sc25519_mul	ssh_crypto_sign_ed25519_ref_sc25519_mul
#define crypto_sign_ed25519_ref_sc25519_mul_shortsc ssh_crypto_sign_ed25519_ref_sc25519_mul_shortsc
#define crypto_sign_ed25519_ref_sc25519_sub_nored ssh_crypto_sign_ed25519_ref_sc25519_sub_nored
#define crypto_sign_ed25519_ref_sc25519_to32bytes ssh_crypto_sign_ed25519_ref_sc25519_to32bytes
#define crypto_sign_ed25519_ref_sc25519_window3 ssh_crypto_sign_ed25519_ref_sc25519_window3
#define crypto_sign_ed25519_ref_sc25519_window5 ssh_crypto_sign_ed25519_ref_sc25519_window5
#define crypto_sign_ed25519_ref_scalarmult_base ssh_crypto_sign_ed25519_ref_scalarmult_base
#define crypto_sign_ed25519_ref_shortsc25519_from16bytes ssh_crypto_sign_ed25519_ref_shortsc25519_from16bytes
#define crypto_sign_ed25519_ref_unpackneg_vartime ssh_crypto_sign_ed25519_ref_unpackneg_vartime
#define crypto_verify_32			ssh_crypto_verify_32
#define dbl_p1p1				ssh_dbl_p1p1
#define debug					ssh_debug
#define debug2					ssh_debug2
#define debug3					ssh_debug3
#define decode_reply				ssh_decode_reply
#define deny_input_open				ssh_deny_input_open
#define derive_ssh1_session_id			ssh_derive_ssh1_session_id
#define detect_attack				ssh_detect_attack
#define dh_estimate				ssh_dh_estimate
#define dh_gen_key				ssh_dh_gen_key
#define dh_new_group				ssh_dh_new_group
#define dh_new_group1				ssh_dh_new_group1
#define dh_new_group14				ssh_dh_new_group14
#define dh_new_group_asc			ssh_dh_new_group_asc
#define dh_pub_is_valid				ssh_dh_pub_is_valid
#define dispatch_init				ssh_dispatch_init
#define dispatch_protocol_error			ssh_dispatch_protocol_error
#define dispatch_protocol_ignore		ssh_dispatch_protocol_ignore
#define dispatch_range				ssh_dispatch_range
#define dispatch_run				ssh_dispatch_run
#define dispatch_set				ssh_dispatch_set
#define do_log					ssh_do_log
#define do_log2					ssh_do_log2
#define dump_base64				ssh_dump_base64
#define enable_compat13				ssh_enable_compat13
#define enable_compat20				ssh_enable_compat20
#define error					ssh_error
#define evp_ssh1_3des				ssh_evp_ssh1_3des
#define evp_ssh1_bf				ssh_evp_ssh1_bf
#define export_dns_rr				ssh_export_dns_rr
#define fatal					ssh_fatal
#define filter_proposal				ssh_filter_proposal
#define fmt_scaled				ssh_fmt_scaled
#define free_hostkeys				ssh_free_hostkeys
#define freeargs				ssh_freeargs
#define freerrset				ssh_freerrset
#define gen_candidates				ssh_gen_candidates
#define get_canonical_hostname			ssh_get_canonical_hostname
#define get_local_ipaddr			ssh_get_local_ipaddr
#define get_local_name				ssh_get_local_name
#define get_local_port				ssh_get_local_port
#define get_peer_ipaddr				ssh_get_peer_ipaddr
#define get_peer_port				ssh_get_peer_port
#define get_remote_ipaddr			ssh_get_remote_ipaddr
#define get_remote_name_or_ip			ssh_get_remote_name_or_ip
#define get_remote_port				ssh_get_remote_port
#define get_sock_port				ssh_get_sock_port
#define get_socket_address			ssh_get_socket_address
#define get_u16					ssh_get_u16
#define get_u32					ssh_get_u32
#define get_u32_le				ssh_get_u32_le
#define get_u64					ssh_get_u64
#define getrrsetbyname				ssh_getrrsetbyname
#define glob					ssh_glob
#define glob0					ssh_glob0
#define glob2					ssh_glob2
#define globexp1				ssh_globexp1
#define globextend				ssh_globextend
#define globfree				ssh_globfree
#define host_hash				ssh_host_hash
#define hostfile_read_key			ssh_hostfile_read_key
#define hpdelim					ssh_hpdelim
#define init_hostkeys				ssh_init_hostkeys
#define iptos2str				ssh_iptos2str
#define ipv64_normalise_mapped			ssh_ipv64_normalise_mapped
#define is_key_revoked				ssh_is_key_revoked
#define kex_alg_by_name				ssh_kex_alg_by_name
#define kex_alg_list				ssh_kex_alg_list
#define kex_buf2prop				ssh_kex_buf2prop
#define kex_c25519_hash				ssh_kex_c25519_hash
#define kex_derive_keys				ssh_kex_derive_keys
#define kex_derive_keys_bn			ssh_kex_derive_keys_bn
#define kex_dh_hash				ssh_kex_dh_hash
#define kex_ecdh_hash				ssh_kex_ecdh_hash
#define kex_finish				ssh_kex_finish
#define kex_get_newkeys				ssh_kex_get_newkeys
#define kex_input_kexinit			ssh_kex_input_kexinit
#define kex_names_valid				ssh_kex_names_valid
#define kex_prop_free				ssh_kex_prop_free
#define kex_protocol_error			ssh_kex_protocol_error
#define kex_send_kexinit			ssh_kex_send_kexinit
#define kex_setup				ssh_kex_setup
#define kexc25519_client			ssh_kexc25519_client
#define kexc25519_keygen			ssh_kexc25519_keygen
#define kexc25519_shared_key			ssh_kexc25519_shared_key
#define kexdh_client				ssh_kexdh_client
#define kexecdh_client				ssh_kexecdh_client
#define kexgex_client				ssh_kexgex_client
#define kexgex_hash				ssh_kexgex_hash
#define key_add_private				ssh_key_add_private
#define key_alg_list				ssh_key_alg_list
#define key_cert_check_authority		ssh_key_cert_check_authority
#define key_cert_copy				ssh_key_cert_copy
#define key_certify				ssh_key_certify
#define key_demote				ssh_key_demote
#define key_drop_cert				ssh_key_drop_cert
#define key_ec_validate_private			ssh_key_ec_validate_private
#define key_ec_validate_public			ssh_key_ec_validate_public
#define key_fingerprint_raw			ssh_key_fingerprint_raw
#define key_from_blob				ssh_key_from_blob
#define key_from_private			ssh_key_from_private
#define key_generate				ssh_key_generate
#define key_in_file				ssh_key_in_file
#define key_load_cert				ssh_key_load_cert
#define key_load_file				ssh_key_load_file
#define key_load_private			ssh_key_load_private
#define key_load_private_cert			ssh_key_load_private_cert
#define key_load_private_pem			ssh_key_load_private_pem
#define key_load_private_type			ssh_key_load_private_type
#define key_load_public				ssh_key_load_public
#define key_new_private				ssh_key_new_private
#define key_perm_ok				ssh_key_perm_ok
#define key_private_deserialize			ssh_key_private_deserialize
#define key_private_serialize			ssh_key_private_serialize
#define key_read				ssh_key_read
#define key_save_private			ssh_key_save_private
#define key_sign				ssh_key_sign
#define key_to_blob				ssh_key_to_blob
#define key_to_certified			ssh_key_to_certified
#define key_verify				ssh_key_verify
#define key_write				ssh_key_write
#define load_hostkeys				ssh_load_hostkeys
#define log_change_level			ssh_log_change_level
#define log_facility_name			ssh_log_facility_name
#define log_facility_number			ssh_log_facility_number
#define log_init				ssh_log_init
#define log_is_on_stderr			ssh_log_is_on_stderr
#define log_level_name				ssh_log_level_name
#define log_level_number			ssh_log_level_number
#define log_redirect_stderr_to			ssh_log_redirect_stderr_to
#define logit					ssh_logit
#define lookup_key_in_hostkeys_by_type		ssh_lookup_key_in_hostkeys_by_type
#define lowercase				ssh_lowercase
#define mac_alg_list				ssh_mac_alg_list
#define mac_clear				ssh_mac_clear
#define mac_compute				ssh_mac_compute
#define mac_init				ssh_mac_init
#define mac_setup				ssh_mac_setup
#define mac_valid				ssh_mac_valid
#define match					ssh_match
#define match_host_and_ip			ssh_match_host_and_ip
#define match_hostname				ssh_match_hostname
#define match_list				ssh_match_list
#define match_pattern				ssh_match_pattern
#define match_pattern_list			ssh_match_pattern_list
#define match_user				ssh_match_user
#define mktemp_proto				ssh_mktemp_proto
#define mm_receive_fd				ssh_mm_receive_fd
#define mm_send_fd				ssh_mm_send_fd
#define monotime				ssh_monotime
#define ms_subtract_diff			ssh_ms_subtract_diff
#define ms_to_timeval				ssh_ms_to_timeval
#define mult					ssh_mult
#define mysignal				ssh_mysignal
#define nh_aux					ssh_nh_aux
#define nh_final				ssh_nh_final
#define packet_add_padding			ssh_packet_add_padding
#define packet_backup_state			ssh_packet_backup_state
#define packet_close				ssh_packet_close
#define packet_connection_is_on_socket		ssh_packet_connection_is_on_socket
#define packet_disconnect			ssh_packet_disconnect
#define packet_enable_delayed_compress		ssh_packet_enable_delayed_compress
#define packet_get_bignum			ssh_packet_get_bignum
#define packet_get_bignum2			ssh_packet_get_bignum2
#define packet_get_char				ssh_packet_get_char
#define packet_get_connection_in		ssh_packet_get_connection_in
#define packet_get_connection_out		ssh_packet_get_connection_out
#define packet_get_cstring			ssh_packet_get_cstring
#define packet_get_ecpoint			ssh_packet_get_ecpoint
#define packet_get_encryption_key		ssh_packet_get_encryption_key
#define packet_get_input			ssh_packet_get_input
#define packet_get_int				ssh_packet_get_int
#define packet_get_int64			ssh_packet_get_int64
#define packet_get_keycontext			ssh_packet_get_keycontext
#define packet_get_keyiv			ssh_packet_get_keyiv
#define packet_get_keyiv_len			ssh_packet_get_keyiv_len
#define packet_get_maxsize			ssh_packet_get_maxsize
#define packet_get_newkeys			ssh_packet_get_newkeys
#define packet_get_output			ssh_packet_get_output
#define packet_get_protocol_flags		ssh_packet_get_protocol_flags
#define packet_get_raw				ssh_packet_get_raw
#define packet_get_rekey_timeout		ssh_packet_get_rekey_timeout
#define packet_get_ssh1_cipher			ssh_packet_get_ssh1_cipher
#define packet_get_state			ssh_packet_get_state
#define packet_get_string			ssh_packet_get_string
#define packet_get_string_ptr			ssh_packet_get_string_ptr
#define packet_have_data_to_write		ssh_packet_have_data_to_write
#define packet_inc_alive_timeouts		ssh_packet_inc_alive_timeouts
#define packet_init_compression			ssh_packet_init_compression
#define packet_is_interactive			ssh_packet_is_interactive
#define packet_need_rekeying			ssh_packet_need_rekeying
#define packet_not_very_much_data_to_write	ssh_packet_not_very_much_data_to_write
#define packet_process_incoming			ssh_packet_process_incoming
#define packet_put_bignum			ssh_packet_put_bignum
#define packet_put_bignum2			ssh_packet_put_bignum2
#define packet_put_char				ssh_packet_put_char
#define packet_put_cstring			ssh_packet_put_cstring
#define packet_put_ecpoint			ssh_packet_put_ecpoint
#define packet_put_int				ssh_packet_put_int
#define packet_put_int64			ssh_packet_put_int64
#define packet_put_raw				ssh_packet_put_raw
#define packet_put_string			ssh_packet_put_string
#define packet_read				ssh_packet_read
#define packet_read_expect			ssh_packet_read_expect
#define packet_read_poll_seqnr			ssh_packet_read_poll_seqnr
#define packet_read_seqnr			ssh_packet_read_seqnr
#define packet_remaining			ssh_packet_remaining
#define packet_restore_state			ssh_packet_restore_state
#define packet_send				ssh_packet_send
#define packet_send2_wrapped			ssh_packet_send2_wrapped
#define packet_send_debug			ssh_packet_send_debug
#define packet_send_ignore			ssh_packet_send_ignore
#define packet_set_alive_timeouts		ssh_packet_set_alive_timeouts
#define packet_set_authenticated		ssh_packet_set_authenticated
#define packet_set_connection			ssh_packet_set_connection
#define packet_set_encryption_key		ssh_packet_set_encryption_key
#define packet_set_interactive			ssh_packet_set_interactive
#define packet_set_iv				ssh_packet_set_iv
#define packet_set_keycontext			ssh_packet_set_keycontext
#define packet_set_maxsize			ssh_packet_set_maxsize
#define packet_set_nonblocking			ssh_packet_set_nonblocking
#define packet_set_postauth			ssh_packet_set_postauth
#define packet_set_protocol_flags		ssh_packet_set_protocol_flags
#define packet_set_rekey_limits			ssh_packet_set_rekey_limits
#define packet_set_server			ssh_packet_set_server
#define packet_set_state			ssh_packet_set_state
#define packet_set_timeout			ssh_packet_set_timeout
#define packet_start				ssh_packet_start
#define packet_start_compression		ssh_packet_start_compression
#define packet_start_discard			ssh_packet_start_discard
#define packet_stop_discard			ssh_packet_stop_discard
#define packet_write_poll			ssh_packet_write_poll
#define packet_write_wait			ssh_packet_write_wait
#define parse_ipqos				ssh_parse_ipqos
#define parse_prime				ssh_parse_prime
#define percent_expand				ssh_percent_expand
#define permanently_drop_suid			ssh_permanently_drop_suid
#define permanently_set_uid			ssh_permanently_set_uid
#define permitopen_port				ssh_permitopen_port
#define pkcs11_add_provider			ssh_pkcs11_add_provider
#define pkcs11_del_provider			ssh_pkcs11_del_provider
#define pkcs11_fetch_keys_filter		ssh_pkcs11_fetch_keys_filter
#define pkcs11_find				ssh_pkcs11_find
#define pkcs11_init				ssh_pkcs11_init
#define pkcs11_provider_finalize		ssh_pkcs11_provider_finalize
#define pkcs11_provider_unref			ssh_pkcs11_provider_unref
#define pkcs11_rsa_finish			ssh_pkcs11_rsa_finish
#define pkcs11_rsa_private_decrypt		ssh_pkcs11_rsa_private_decrypt
#define pkcs11_rsa_private_encrypt		ssh_pkcs11_rsa_private_encrypt
#define pkcs11_terminate			ssh_pkcs11_terminate
#define plain_key_blob				ssh_plain_key_blob
#define poly1305_auth				ssh_poly1305_auth
#define poly64					ssh_poly64
#define poly_hash				ssh_poly_hash
#define port_open_helper			ssh_port_open_helper
#define prime_test				ssh_prime_test
#define proto_spec				ssh_proto_spec
#define put_host_port				ssh_put_host_port
#define put_u16					ssh_put_u16
#define put_u32					ssh_put_u32
#define put_u32_le				ssh_put_u32_le
#define put_u64					ssh_put_u64
#define pwcopy					ssh_pwcopy
#define qfileout				ssh_qfileout
#define read_keyfile_line			ssh_read_keyfile_line
#define read_mux				ssh_read_mux
#define read_passphrase				ssh_read_passphrase
#define reduce_add_sub				ssh_reduce_add_sub
#define refresh_progress_meter			ssh_refresh_progress_meter
#define replacearg				ssh_replacearg
#define restore_uid				ssh_restore_uid
#define revoke_blob				ssh_revoke_blob
#define revoked_blob_tree_RB_REMOVE		ssh_revoked_blob_tree_RB_REMOVE
#define revoked_certs_for_ca_key		ssh_revoked_certs_for_ca_key
#define revoked_serial_tree_RB_REMOVE		ssh_revoked_serial_tree_RB_REMOVE
#define rijndaelEncrypt				ssh_rijndaelEncrypt
#define rijndaelKeySetupDec			ssh_rijndaelKeySetupDec
#define rijndaelKeySetupEnc			ssh_rijndaelKeySetupEnc
#define rijndael_decrypt			ssh_rijndael_decrypt
#define rijndael_encrypt			ssh_rijndael_encrypt
#define rijndael_set_key			ssh_rijndael_set_key
#define rsa_generate_additional_parameters	ssh_rsa_generate_additional_parameters
#define rsa_private_decrypt			ssh_rsa_private_decrypt
#define rsa_public_encrypt			ssh_rsa_public_encrypt
#define sanitise_stdfd				ssh_sanitise_stdfd
#define scan_scaled				ssh_scan_scaled
#define seed_rng				ssh_seed_rng
#define set_log_handler				ssh_set_log_handler
#define set_newkeys				ssh_set_newkeys
#define set_nodelay				ssh_set_nodelay
#define set_nonblock				ssh_set_nonblock
#define shadow_pw				ssh_shadow_pw
#define sieve_large				ssh_sieve_large
#define sig_winch				ssh_sig_winch
#define sigdie					ssh_sigdie
#define sock_set_v6only				ssh_sock_set_v6only
#define square					ssh_square
#define ssh1_3des_cbc				ssh_ssh1_3des_cbc
#define ssh1_3des_cleanup			ssh_ssh1_3des_cleanup
#define ssh1_3des_init				ssh_ssh1_3des_init
#define ssh1_3des_iv				ssh_ssh1_3des_iv
#define sshbuf_alloc				ssh_sshbuf_alloc
#define sshbuf_avail				ssh_sshbuf_avail
#define sshbuf_b64tod				ssh_sshbuf_b64tod
#define sshbuf_check_reserve			ssh_sshbuf_check_reserve
#define sshbuf_consume				ssh_sshbuf_consume
#define sshbuf_consume_end			ssh_sshbuf_consume_end
#define sshbuf_dtob16				ssh_sshbuf_dtob16
#define sshbuf_dtob64				ssh_sshbuf_dtob64
#define sshbuf_dump				ssh_sshbuf_dump
#define sshbuf_dump_data			ssh_sshbuf_dump_data
#define sshbuf_free				ssh_sshbuf_free
#define sshbuf_from				ssh_sshbuf_from
#define sshbuf_fromb				ssh_sshbuf_fromb
#define sshbuf_froms				ssh_sshbuf_froms
#define sshbuf_get				ssh_sshbuf_get
#define sshbuf_get_bignum1			ssh_sshbuf_get_bignum1
#define sshbuf_get_bignum2			ssh_sshbuf_get_bignum2
#define sshbuf_get_cstring			ssh_sshbuf_get_cstring
#define sshbuf_get_ec				ssh_sshbuf_get_ec
#define sshbuf_get_eckey			ssh_sshbuf_get_eckey
#define sshbuf_get_string			ssh_sshbuf_get_string
#define sshbuf_get_string_direct		ssh_sshbuf_get_string_direct
#define sshbuf_get_stringb			ssh_sshbuf_get_stringb
#define sshbuf_get_u16				ssh_sshbuf_get_u16
#define sshbuf_get_u32				ssh_sshbuf_get_u32
#define sshbuf_get_u64				ssh_sshbuf_get_u64
#define sshbuf_get_u8				ssh_sshbuf_get_u8
#define sshbuf_init				ssh_sshbuf_init
#define sshbuf_len				ssh_sshbuf_len
#define sshbuf_max_size				ssh_sshbuf_max_size
#define sshbuf_mutable_ptr			ssh_sshbuf_mutable_ptr
#define sshbuf_new				ssh_sshbuf_new
#define sshbuf_parent				ssh_sshbuf_parent
#define sshbuf_peek_string_direct		ssh_sshbuf_peek_string_direct
#define sshbuf_ptr				ssh_sshbuf_ptr
#define sshbuf_put				ssh_sshbuf_put
#define sshbuf_put_bignum1			ssh_sshbuf_put_bignum1
#define sshbuf_put_bignum2			ssh_sshbuf_put_bignum2
#define sshbuf_put_bignum2_bytes		ssh_sshbuf_put_bignum2_bytes
#define sshbuf_put_cstring			ssh_sshbuf_put_cstring
#define sshbuf_put_ec				ssh_sshbuf_put_ec
#define sshbuf_put_eckey			ssh_sshbuf_put_eckey
#define sshbuf_put_string			ssh_sshbuf_put_string
#define sshbuf_put_stringb			ssh_sshbuf_put_stringb
#define sshbuf_put_u16				ssh_sshbuf_put_u16
#define sshbuf_put_u32				ssh_sshbuf_put_u32
#define sshbuf_put_u64				ssh_sshbuf_put_u64
#define sshbuf_put_u8				ssh_sshbuf_put_u8
#define sshbuf_putb				ssh_sshbuf_putb
#define sshbuf_putf				ssh_sshbuf_putf
#define sshbuf_putfv				ssh_sshbuf_putfv
#define sshbuf_refcount				ssh_sshbuf_refcount
#define sshbuf_reserve				ssh_sshbuf_reserve
#define sshbuf_reset				ssh_sshbuf_reset
#define sshbuf_set_max_size			ssh_sshbuf_set_max_size
#define sshbuf_set_parent			ssh_sshbuf_set_parent
#define sshkey_add_private			ssh_sshkey_add_private
#define sshkey_cert_check_authority		ssh_sshkey_cert_check_authority
#define sshkey_cert_copy			ssh_sshkey_cert_copy
#define sshkey_cert_is_legacy			ssh_sshkey_cert_is_legacy
#define sshkey_cert_type			ssh_sshkey_cert_type
#define sshkey_certify				ssh_sshkey_certify
#define sshkey_curve_name_to_nid		ssh_sshkey_curve_name_to_nid
#define sshkey_curve_nid_to_bits		ssh_sshkey_curve_nid_to_bits
#define sshkey_curve_nid_to_name		ssh_sshkey_curve_nid_to_name
#define sshkey_demote				ssh_sshkey_demote
#define sshkey_drop_cert			ssh_sshkey_drop_cert
#define sshkey_dump_ec_key			ssh_sshkey_dump_ec_key
#define sshkey_dump_ec_point			ssh_sshkey_dump_ec_point
#define sshkey_ec_nid_to_hash_alg		ssh_sshkey_ec_nid_to_hash_alg
#define sshkey_ec_validate_private		ssh_sshkey_ec_validate_private
#define sshkey_ec_validate_public		ssh_sshkey_ec_validate_public
#define sshkey_ecdsa_bits_to_nid		ssh_sshkey_ecdsa_bits_to_nid
#define sshkey_ecdsa_key_to_nid			ssh_sshkey_ecdsa_key_to_nid
#define sshkey_ecdsa_nid_from_name		ssh_sshkey_ecdsa_nid_from_name
#define sshkey_equal				ssh_sshkey_equal
#define sshkey_equal_public			ssh_sshkey_equal_public
#define sshkey_fingerprint			ssh_sshkey_fingerprint
#define sshkey_fingerprint_raw			ssh_sshkey_fingerprint_raw
#define sshkey_free				ssh_sshkey_free
#define sshkey_from_blob			ssh_sshkey_from_blob
#define sshkey_from_blob_internal		ssh_sshkey_from_blob_internal
#define sshkey_from_private			ssh_sshkey_from_private
#define sshkey_generate				ssh_sshkey_generate
#define sshkey_in_file				ssh_sshkey_in_file
#define sshkey_is_cert				ssh_sshkey_is_cert
#define sshkey_load_cert			ssh_sshkey_load_cert
#define sshkey_load_file			ssh_sshkey_load_file
#define sshkey_load_private			ssh_sshkey_load_private
#define sshkey_load_private_cert		ssh_sshkey_load_private_cert
#define sshkey_load_private_pem			ssh_sshkey_load_private_pem
#define sshkey_load_private_type		ssh_sshkey_load_private_type
#define sshkey_load_public			ssh_sshkey_load_public
#define sshkey_names_valid2			ssh_sshkey_names_valid2
#define sshkey_new				ssh_sshkey_new
#define sshkey_new_private			ssh_sshkey_new_private
#define sshkey_parse_private2			ssh_sshkey_parse_private2
#define sshkey_parse_private_fileblob		ssh_sshkey_parse_private_fileblob
#define sshkey_parse_private_fileblob_type	ssh_sshkey_parse_private_fileblob_type
#define sshkey_parse_private_pem_fileblob	ssh_sshkey_parse_private_pem_fileblob
#define sshkey_parse_public_rsa1_fileblob	ssh_sshkey_parse_public_rsa1_fileblob
#define sshkey_perm_ok				ssh_sshkey_perm_ok
#define sshkey_plain_to_blob			ssh_sshkey_plain_to_blob
#define sshkey_plain_to_blob_buf		ssh_sshkey_plain_to_blob_buf
#define sshkey_private_deserialize		ssh_sshkey_private_deserialize
#define sshkey_private_serialize		ssh_sshkey_private_serialize
#define sshkey_private_to_blob2			ssh_sshkey_private_to_blob2
#define sshkey_private_to_fileblob		ssh_sshkey_private_to_fileblob
#define sshkey_read				ssh_sshkey_read
#define sshkey_save_private			ssh_sshkey_save_private
#define sshkey_sign				ssh_sshkey_sign
#define sshkey_size				ssh_sshkey_size
#define sshkey_ssh_name				ssh_sshkey_ssh_name
#define sshkey_ssh_name_plain			ssh_sshkey_ssh_name_plain
#define sshkey_to_blob				ssh_sshkey_to_blob
#define sshkey_to_blob_buf			ssh_sshkey_to_blob_buf
#define sshkey_to_certified			ssh_sshkey_to_certified
#define sshkey_try_load_public			ssh_sshkey_try_load_public
#define sshkey_type				ssh_sshkey_type
#define sshkey_type_from_name			ssh_sshkey_type_from_name
#define sshkey_type_is_cert			ssh_sshkey_type_is_cert
#define sshkey_type_plain			ssh_sshkey_type_plain
#define sshkey_verify				ssh_sshkey_verify
#define sshkey_write				ssh_sshkey_write
#define start_progress_meter			ssh_start_progress_meter
#define stop_progress_meter			ssh_stop_progress_meter
#define strdelim				ssh_strdelim
#define strnvis					ssh_strnvis
#define strvis					ssh_strvis
#define strvisx					ssh_strvisx
#define sys_tun_open				ssh_sys_tun_open
#define temporarily_use_uid			ssh_temporarily_use_uid
#define tilde_expand_filename			ssh_tilde_expand_filename
#define timingsafe_bcmp				ssh_timingsafe_bcmp
#define to_blob					ssh_to_blob
#define to_blob_buf				ssh_to_blob_buf
#define tohex					ssh_tohex
#define tty_make_modes				ssh_tty_make_modes
#define tty_parse_modes				ssh_tty_parse_modes
#define tun_open				ssh_tun_open
#define umac128_delete				ssh_umac128_delete
#define umac128_final				ssh_umac128_final
#define umac128_new				ssh_umac128_new
#define umac128_update				ssh_umac128_update
#define umac_delete				ssh_umac_delete
#define umac_final				ssh_umac_final
#define umac_new				ssh_umac_new
#define umac_update				ssh_umac_update
#define unix_listener				ssh_unix_listener
#define unset_nonblock				ssh_unset_nonblock
#define update_progress_meter			ssh_update_progress_meter
#define uudecode				ssh_uudecode
#define uuencode				ssh_uuencode
#define verbose					ssh_verbose
#define verify_host_key_dns			ssh_verify_host_key_dns
#define vis					ssh_vis
#define x11_connect_display			ssh_x11_connect_display
#define x11_create_display_inet			ssh_x11_create_display_inet
#define x11_input_open				ssh_x11_input_open
#define x11_open_helper				ssh_x11_open_helper
#define x11_request_forwarding_with_spoofing	ssh_x11_request_forwarding_with_spoofing
#define xasprintf				ssh_xasprintf
#define xcalloc					ssh_xcalloc
#define xcrypt					ssh_xcrypt
#define xmalloc					ssh_xmalloc
#define xmmap					ssh_xmmap
#define xrealloc				ssh_xrealloc
#define xstrdup					ssh_xstrdup
