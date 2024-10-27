#pragma once

#include <psyqo/gte-kernels.hh>
#include <psyqo/gte-registers.hh>

namespace psyqo::GTE::Math {
    template <psyqo::GTE::PseudoRegister reg>
    constexpr psyqo::GTE::Kernels::MX getMX() {
        if constexpr (reg == psyqo::GTE::PseudoRegister::Rotation) {
            return psyqo::GTE::Kernels::MX::RT;
        } else if constexpr (reg == psyqo::GTE::PseudoRegister::Light) {
            return psyqo::GTE::Kernels::MX::LL;
        } else if constexpr (reg == psyqo::GTE::PseudoRegister::Color) {
            return psyqo::GTE::Kernels::MX::LC;
        } else {
            static_assert(false, "Not a matrix pseudo register (should be Rotation, Light or Color)");
        }
    }

    template <psyqo::GTE::PseudoRegister reg>
    constexpr psyqo::GTE::Kernels::MV getMV() {
        if constexpr (reg == psyqo::GTE::PseudoRegister::V0) {
            return psyqo::GTE::Kernels::MV::V0;
        } else if constexpr (reg == psyqo::GTE::PseudoRegister::V1) {
            return psyqo::GTE::Kernels::MV::V1;
        } else if constexpr (reg == psyqo::GTE::PseudoRegister::V2) {
            return psyqo::GTE::Kernels::MV::V2;
        } else {
            static_assert(false, "Not a valid pseudo register (should be V0, V1 or V2)");
        }
    }

    // multiply m by v and store the result in out
    // m is already stored in mreg
    template <psyqo::GTE::PseudoRegister mreg, psyqo::GTE::PseudoRegister vreg>
    void matrixVecMul3(const psyqo::Vec3& v, psyqo::Vec3* out) {
        constexpr auto mx = getMX<mreg>();
        constexpr auto mv = getMV<vreg>();

        psyqo::GTE::writeSafe<vreg>(v);
        psyqo::GTE::Kernels::mvmva<mx, mv>();

        *out = psyqo::GTE::readSafe<psyqo::GTE::PseudoRegister::SV>();
    }

    // multiply m by v and store the result in out
    template <psyqo::GTE::PseudoRegister mreg, psyqo::GTE::PseudoRegister vreg>
    void matrixVecMul3(const psyqo::Matrix33& m, const psyqo::Vec3& v, psyqo::Vec3* out) {
        psyqo::GTE::writeUnsafe<mreg>(m);
        matrixVecMul3<mreg, vreg>(v, out);
    }

    // multiply m1 by m2 and store the result in out
    // m1 is already stored in mreg
    template <psyqo::GTE::PseudoRegister mreg, psyqo::GTE::PseudoRegister vreg>
    void multiplyMatrix33(const psyqo::Matrix33& m2, psyqo::Matrix33* out) {
        constexpr auto mx = getMX<mreg>();
        constexpr auto mv = getMV<vreg>();

        psyqo::Vec3 t;

        t.x = m2.vs[0].x;
        t.y = m2.vs[1].x;
        t.z = m2.vs[2].x;
        psyqo::GTE::writeSafe<vreg>(t);

        psyqo::GTE::Kernels::mvmva<mx, mv>();

        t = psyqo::GTE::readSafe<psyqo::GTE::PseudoRegister::SV>();
        out->vs[0].x = t.x;
        out->vs[1].x = t.y;
        out->vs[2].x = t.z;

        t.x = m2.vs[0].y;
        t.y = m2.vs[1].y;
        t.z = m2.vs[2].y;
        psyqo::GTE::writeSafe<vreg>(t);

        psyqo::GTE::Kernels::mvmva<mx, mv>();
        
        t = psyqo::GTE::readSafe<psyqo::GTE::PseudoRegister::SV>();
        out->vs[0].y = t.x;
        out->vs[1].y = t.y;
        out->vs[2].y = t.z;

        t.x = m2.vs[0].z;
        t.y = m2.vs[1].z;
        t.z = m2.vs[2].z;
        psyqo::GTE::writeSafe<vreg>(t);

        psyqo::GTE::Kernels::mvmva<mx, mv>();

        t = psyqo::GTE::readSafe<psyqo::GTE::PseudoRegister::SV>();
        out->vs[0].z = t.x;
        out->vs[1].z = t.y;
        out->vs[2].z = t.z;
    }

    // multiply m1 by m2 and store the result in out
    template <psyqo::GTE::PseudoRegister mreg, psyqo::GTE::PseudoRegister vreg>
    void multiplyMatrix33(const psyqo::Matrix33& m1, const psyqo::Matrix33& m2, psyqo::Matrix33* out) {
        psyqo::GTE::writeUnsafe<mreg>(m1);
        multiplyMatrix33<mreg, vreg>(m2, out);
    }
}
