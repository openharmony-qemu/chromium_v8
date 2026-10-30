// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "include/v8-script.h"
#include "src/api/api-inl.h"
#include "src/base/memory.h"
#include "src/codegen/compilation-cache.h"
#include "src/heap/factory-inl.h"
#include "src/objects/script-inl.h"
#include "src/objects/shared-function-info-inl.h"
#include "src/objects/swiss-name-dictionary-inl.h"
#include "src/roots/roots.h"
#include "src/snapshot/code-serializer.h"
#include "src/snapshot/serializer-deserializer.h"
#include "src/snapshot/snapshot-source-sink.h"
#include "src/snapshot/snapshot-utils.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

#ifdef V8_ENABLE_SANDBOX

namespace v8::internal {

// Official builds omit CHECK messages.
#ifdef OFFICIAL_BUILD
#define EXPECTED_CHECK_WITH_MSG(msg) ""
#else
#define EXPECTED_CHECK_WITH_MSG(msg) msg
#endif

namespace {

constexpr char kSource[] = "function f() { return 42; }; f();";

struct Bytecodes : public SerializerDeserializer {
  using SerializerDeserializer::Bytecode;
  using enum Bytecode;
};

void SetHeaderValue(std::vector<uint8_t>* data, uint32_t offset,
                    uint32_t value) {
  base::WriteLittleEndianValue<uint32_t>(
      reinterpret_cast<Address>(data->data() + offset), value);
}

}  // namespace

class CodeCacheBoundsTest : public TestWithContext {
 protected:
  v8::Local<v8::UnboundScript> Compile() {
    i_isolate()->compilation_cache()->DisableScriptAndEval();
    v8::ScriptCompiler::Source source(NewString(kSource));
    return v8::ScriptCompiler::CompileUnboundScript(isolate(), &source)
        .ToLocalChecked();
  }

  std::vector<uint8_t> MakeCodeCache(const SnapshotByteSink& root) {
    std::unique_ptr<v8::ScriptCompiler::CachedData> original(
        v8::ScriptCompiler::CreateCodeCache(Compile()));
    CHECK_NOT_NULL(original);
    // V8 14.4 has one payload; it does not have trusted/untrusted sections.
    std::vector<uint8_t> data(original->data,
                              original->data + SerializedCodeData::kHeaderSize);
    data.insert(data.end(), root.data()->begin(), root.data()->end());
    data.push_back(Bytecodes::kSynchronize);
    // Match Serializer::Pad, including GetUint30's four-byte read-ahead.
    data.insert(data.end(), 3, Bytecodes::kNop);
    while (!IsAligned(data.size(), kPointerAlignment)) {
      data.push_back(Bytecodes::kNop);
    }
    base::Vector<const uint8_t> payload(
        data.data() + SerializedCodeData::kHeaderSize,
        data.size() - SerializedCodeData::kHeaderSize);
    SetHeaderValue(&data, SerializedCodeData::kPayloadLengthOffset,
                     static_cast<uint32_t>(payload.size()));
    SetHeaderValue(&data, SerializedCodeData::kChecksumOffset, Checksum(payload));
    return data;
  }

  void Consume(const std::vector<uint8_t>& data) {
    v8::ScriptCompiler::Source source(
        NewString(kSource), new v8::ScriptCompiler::CachedData(
                                data.data(), static_cast<int>(data.size())));
    USE(v8::ScriptCompiler::CompileUnboundScript(
        isolate(), &source, v8::ScriptCompiler::kConsumeCodeCache));
  }
};

TEST_F(CodeCacheBoundsTest, UnalignedSwissNameDictionary) {
  Handle<SharedFunctionInfo> shared = Utils::OpenHandle(*Compile());
  Handle<SwissNameDictionary> dictionary =
      i_isolate()->factory()->NewSwissNameDictionary();
  ASSERT_DEATH_IF_SUPPORTED(
      {
        // Use the target version's real map and layout. Capacity 1 makes the
        // computed size unaligned, the stream-desynchronization primitive in
        // issue 566142291. Only the death-test child sees the corrupt object.
        Cast<Script>(shared->script())->set_name(*dictionary);
        dictionary->WriteField<int>(SwissNameDictionary::CapacityOffset(), 1);
        std::unique_ptr<v8::ScriptCompiler::CachedData> cache(
            CodeSerializer::Serialize(i_isolate(), shared));
      },
      // Debug may reject the invalid capacity before reaching SerializePrologue.
      // Release must reject the unaligned size via the new SBXCHECK.
      EXPECTED_CHECK_WITH_MSG("IsValidCapacity|IsAligned"));
}

TEST_F(CodeCacheBoundsTest, RootIndexOutOfBounds) {
  SnapshotByteSink root;
  root.Put(Bytecodes::kRootArray, "");
  root.PutUint30(RootsTable::kEntriesCount, "");
  std::vector<uint8_t> cache = MakeCodeCache(root);
  ASSERT_DEATH_IF_SUPPORTED(Consume(cache),
                            EXPECTED_CHECK_WITH_MSG("kEntriesCount"));
}

TEST_F(CodeCacheBoundsTest, BackingStoreLongerThanPayload) {
  for (uint32_t length : {uint32_t{65536}, uint32_t{0xffffffff}}) {
    SnapshotByteSink root;
    root.Put(Bytecodes::kOffHeapBackingStore, "");
    root.PutUint32(length, "");
    std::vector<uint8_t> cache = MakeCodeCache(root);
    ASSERT_DEATH_IF_SUPPORTED(Consume(cache),
                              EXPECTED_CHECK_WITH_MSG("byte_length"));
  }
}

TEST_F(CodeCacheBoundsTest, ResizableBackingStoreLongerThanPayload) {
  SnapshotByteSink root;
  root.Put(Bytecodes::kOffHeapResizableBackingStore, "");
  root.PutUint32(65536, "");
  root.PutUint32(65536, "");
  std::vector<uint8_t> cache = MakeCodeCache(root);
  ASSERT_DEATH_IF_SUPPORTED(Consume(cache),
                            EXPECTED_CHECK_WITH_MSG("byte_length"));
}

TEST(SnapshotByteSourceBoundsTest, TruncatedLengthHeader) {
  const char bytes[] = {0, 0, 0};
  SnapshotByteSource source(bytes, sizeof(bytes));
  ASSERT_DEATH_IF_SUPPORTED(source.GetUint32(),
                            EXPECTED_CHECK_WITH_MSG("number_of_bytes"));
}

TEST(SnapshotByteSourceBoundsTest, TruncatedResizableLengthHeader) {
  const char bytes[] = {0, 0, 0, 0, 0, 0, 0};
  SnapshotByteSource source(bytes, sizeof(bytes));
  EXPECT_EQ(0u, source.GetUint32());
  ASSERT_DEATH_IF_SUPPORTED(source.GetUint32(),
                            EXPECTED_CHECK_WITH_MSG("number_of_bytes"));
}

TEST(SnapshotByteSourceBoundsTest, NegativeCopyLength) {
  const char bytes[] = {0};
  char result = 0;
  SnapshotByteSource source(bytes, sizeof(bytes));
  ASSERT_DEATH_IF_SUPPORTED(source.CopyRaw(&result, -1),
                            EXPECTED_CHECK_WITH_MSG("number_of_bytes"));
}

TEST(SnapshotByteSourceBoundsTest, ExactLengthHeader) {
  const char bytes[] = {0, 0, 0, 0};
  SnapshotByteSource source(bytes, sizeof(bytes));
  EXPECT_EQ(0u, source.GetUint32());
  EXPECT_FALSE(source.HasMore());
}

#undef EXPECTED_CHECK_WITH_MSG

}  // namespace v8::internal

#endif  // V8_ENABLE_SANDBOX
