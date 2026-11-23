`host_shader_pack_legacy.c` is `port/ios/host/host_shader_pack.c` as it was before the shader record contract
(`port/runtime/shader_record.h`), kept byte for byte. `tools/test_shader_record.py` builds it beside the current
validator and requires the two to give the same verdict on every record of a generated corpus, so the contract's
`admit` stays exactly the rules the app has always enforced. Do not edit it; delete it only with the test.
