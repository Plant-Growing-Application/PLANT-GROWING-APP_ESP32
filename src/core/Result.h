#pragma once

// Değer/hata dönüş tipi — TASK-004
//
// KONVANSİYON (CODING_STANDARDS §4):
//
//   Değer döndürmeyen işlem  →  `ErrCode` doğrudan dönülür
//   Değer döndüren işlem     →  `Result<T>` dönülür
//
// C++ istisnaları KULLANILMAZ: arduino-esp32'de varsayılan olarak kapalı ve
// maliyetlidir.
//
// Result<T> neden union değil: C++11'de union tabanlı varyant, elle ctor/dtor
// yönetimi gerektirir ve POD olma özelliğini kaybettirir. Düz `{ErrCode; T;}`
// yapısı küçük T için birkaç bayt israf eder ama POD kalır — bu, StateStore
// snapshot deseni (TASK-007) için zorunludur.

#include "ErrorCodes.h"

#include <type_traits>

namespace core {

template <typename T>
struct Result
{
    static_assert(std::is_trivially_copyable<T>::value,
                  "Result<T>: T trivially copyable olmali (heap/POD kurali)");

    ErrCode error;
    T       value;

    constexpr bool ok() const { return error == ErrCode::OK; }
    constexpr bool failed() const { return error != ErrCode::OK; }

    /// Hata durumunda `fallback` döner. Çağıranın hatayı sessizce yutmasını
    /// kolaylaştırmaz — kullanımı bilinçli bir tercih olmalıdır.
    constexpr T valueOr(T fallback) const { return error == ErrCode::OK ? value : fallback; }
};

/// Başarılı sonuç üretir.
template <typename T>
constexpr Result<T> ok(T v)
{
    return Result<T>{ErrCode::OK, v};
}

/// Hatalı sonuç üretir. Değer alanı tipin varsayılan değerini alır ve
/// okunmamalıdır.
template <typename T>
constexpr Result<T> err(ErrCode e)
{
    return Result<T>{e, T{}};
}

// ---------------------------------------------------------------------------
// Derleme zamanı doğrulama
// ---------------------------------------------------------------------------

static_assert(std::is_trivially_copyable<Result<int32_t>>::value,
              "Result<int32_t> trivially copyable olmali");
static_assert(std::is_trivially_copyable<Result<float>>::value,
              "Result<float> trivially copyable olmali");
static_assert(std::is_standard_layout<Result<int32_t>>::value,
              "Result standard layout olmali");

// Boyut bütçesi: Result state ve kuyruk yapılarında çoğaltılacak.
// 4 baytlık bir değer için toplam 8 baytı aşmamalı (2 bayt kod + hizalama).
static_assert(sizeof(Result<int32_t>) <= 8, "Result<int32_t> 8 bayti asmamali");
static_assert(sizeof(Result<float>) <= 8, "Result<float> 8 bayti asmamali");

// Davranış: ok()/err() doğru sonuç üretiyor mu?
static_assert(ok<int32_t>(42).ok(), "ok() basarili sonuc uretmeli");
static_assert(ok<int32_t>(42).value == 42, "ok() degeri tasimali");
static_assert(err<int32_t>(ErrCode::SENSOR_STALE).failed(), "err() hata uretmeli");
static_assert(err<int32_t>(ErrCode::SENSOR_STALE).error == ErrCode::SENSOR_STALE,
              "err() hata kodunu tasimali");
static_assert(err<int32_t>(ErrCode::SENSOR_STALE).valueOr(-1) == -1,
              "valueOr() hata durumunda fallback donmeli");
static_assert(ok<int32_t>(7).valueOr(-1) == 7, "valueOr() basarida degeri donmeli");

} // namespace core
