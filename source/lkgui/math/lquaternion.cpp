#include "lquaternion.h"

LK_NAMESPACE_BEGIN

float LQuaternion::length() const
{
    return lHypot(xp, yp, zp, wp);
}

float LQuaternion::lengthSquared() const
{
    return xp * xp + yp * yp + zp * zp + wp * wp;
}

LQuaternion LQuaternion::normalized() const
{
    const float scale = length();
    if (lFuzzyIsZero(scale))
        return LQuaternion(0.0f, 0.0f, 0.0f, 0.0f);
    return *this / scale;
}

void LQuaternion::normalize()
{
    const float len = length();
    if (lFuzzyIsZero(len))
        return;

    xp /= len;
    yp /= len;
    zp /= len;
    wp /= len;
}


LVector3D LQuaternion::rotatedVector(const LVector3D &vector) const
{
    return (*this * LQuaternion(0, vector) * conjugated()).vector();
}


LQuaternion LQuaternion::fromAxisAndAngle(const LVector3D &axis, float angle)
{
    // Algorithm from:
    // http://www.j3d.org/matrix_faq/matrfaq_latest.html#Q56
    // We normalize the result just in case the values are close
    // to zero, as suggested in the above FAQ.
    float a = lDegreesToRadians(angle / 2.0f);
    float s = std::sin(a);
    float c = std::cos(a);
    LVector3D ax = axis.normalized();
    return LQuaternion(c, ax.x() * s, ax.y() * s, ax.z() * s).normalized();
}

void LQuaternion::getAxisAndAngle(float *x, float *y, float *z, float *angle) const
{
    // The quaternion representing the rotation is
    //   q = cos(A/2)+sin(A/2)*(x*i+y*j+z*k)

    const float length = std::hypot(xp, yp, zp);
    if (!lFuzzyIsZero(length)) {
        if (lFuzzyCompare(length, 1.0f)) {
            *x = xp;
            *y = yp;
            *z = zp;
        } else {
            *x = xp / length;
            *y = yp / length;
            *z = zp / length;
        }
        *angle = lRadiansToDegrees(2.0f * std::atan2(length, wp));
    } else {
        // angle is 0 (mod 2*pi), so any axis will fit
        *x = *y = *z = *angle = 0.0f;
    }
}

LQuaternion LQuaternion::fromAxisAndAngle
    (float x, float y, float z, float angle)
{
    float length = std::hypot(x, y, z);
    if (!lFuzzyIsZero(length) && !lFuzzyCompare(length, 1.0f)) {
        x /= length;
        y /= length;
        z /= length;
    }
    float a = lDegreesToRadians(angle / 2.0f);
    float s = std::sin(a);
    float c = std::cos(a);
    return LQuaternion(c, x * s, y * s, z * s).normalized();
}

auto LQuaternion::eulerAngles() const -> EulerAngles<float>
{
    EulerAngles<float> result;

    // to avoid churn
    auto pitch = &result.pitch;
    auto yaw = &result.yaw;
    auto roll = &result.roll;

    // Algorithm adapted from:
    // https://ingmec.ual.es/~jlblanco/papers/jlblanco2010geometry3D_techrep.pdf
    // "A tutorial on SE(3) transformation parameterizations and on-manifold optimization".

    // We can only detect Gimbal lock when we normalize, which we can't do when
    // length is nearly zero. Do so before multiplying coordinates, to avoid
    // underflow.
    const float len = length();
    const bool rescale = !lFuzzyIsZero(len);
    const float xps = rescale ? xp / len : xp;
    const float yps = rescale ? yp / len : yp;
    const float zps = rescale ? zp / len : zp;
    const float wps = rescale ? wp / len : wp;

    const float xx = xps * xps;
    const float xy = xps * yps;
    const float xz = xps * zps;
    const float xw = xps * wps;
    const float yy = yps * yps;
    const float yz = yps * zps;
    const float yw = yps * wps;
    const float zz = zps * zps;
    const float zw = zps * wps;

    // For the common case, we have a hidden division by cos(pitch) to calculate
    // yaw and roll: atan2(a / cos(pitch), b / cos(pitch)) = atan2(a, b). This equation
    // wouldn't work if cos(pitch) is close to zero (i.e. abs(sin(pitch)) =~ 1.0).
    // This threshold is copied from lFuzzyIsZero() to avoid the hidden division by zero.
    constexpr float epsilon = 0.00001f;

    const float sinp = -2.0f * (yz - xw);
    if (std::abs(sinp) < 1.0f - epsilon) {
        *pitch = std::asin(sinp);
        *yaw = std::atan2(2.0f * (xz + yw), 1.0f - 2.0f * (xx + yy));
        *roll = std::atan2(2.0f * (xy + zw), 1.0f - 2.0f * (xx + zz));
    } else {
        // Gimbal lock case, which doesn't have a unique solution. We just use
        // XY rotation.
        *pitch = std::copysign(static_cast<float>(std::numbers::pi/2), sinp);
        *yaw = 2.0f * std::atan2(yps, wps);
        *roll = 0.0f;
    }

    *pitch = lRadiansToDegrees(*pitch);
    *yaw = lRadiansToDegrees(*yaw);
    *roll = lRadiansToDegrees(*roll);

    return result;
}

/*!
    \since 5.5

    Creates a quaternion that corresponds to a rotation of
    \a roll degrees around the z axis, \a pitch degrees around the x axis,
    and \a yaw degrees around the y axis (in that order).

    \sa eulerAngles(), toEulerAngles(), fromEulerAngles(LQuaternion::EulerAngles<float>)
*/
LQuaternion LQuaternion::fromEulerAngles(float pitch, float yaw, float roll)
{
    // Algorithm from:
    // http://www.j3d.org/matrix_faq/matrfaq_latest.html#Q60

    pitch = lDegreesToRadians(pitch);
    yaw = lDegreesToRadians(yaw);
    roll = lDegreesToRadians(roll);

    pitch *= 0.5f;
    yaw *= 0.5f;
    roll *= 0.5f;

    const float c1 = std::cos(yaw);
    const float s1 = std::sin(yaw);
    const float c2 = std::cos(roll);
    const float s2 = std::sin(roll);
    const float c3 = std::cos(pitch);
    const float s3 = std::sin(pitch);
    const float c1c2 = c1 * c2;
    const float s1s2 = s1 * s2;

    const float w = c1c2 * c3 + s1s2 * s3;
    const float x = c1c2 * s3 + s1s2 * c3;
    const float y = s1 * c2 * c3 - c1 * s2 * s3;
    const float z = c1 * s2 * c3 - s1 * c2 * s3;

    return LQuaternion(w, x, y, z);
}

LMatrix3x3 LQuaternion::toRotationMatrix() const
{
    // Algorithm from:
    // http://www.j3d.org/matrix_faq/matrfaq_latest.html#Q54

    LMatrix3x3 rot3x3(Uninitialized);

    const float f2x = xp + xp;
    const float f2y = yp + yp;
    const float f2z = zp + zp;
    const float f2xw = f2x * wp;
    const float f2yw = f2y * wp;
    const float f2zw = f2z * wp;
    const float f2xx = f2x * xp;
    const float f2xy = f2x * yp;
    const float f2xz = f2x * zp;
    const float f2yy = f2y * yp;
    const float f2yz = f2y * zp;
    const float f2zz = f2z * zp;

    rot3x3(0, 0) = 1.0f - (f2yy + f2zz);
    rot3x3(0, 1) =         f2xy - f2zw;
    rot3x3(0, 2) =         f2xz + f2yw;
    rot3x3(1, 0) =         f2xy + f2zw;
    rot3x3(1, 1) = 1.0f - (f2xx + f2zz);
    rot3x3(1, 2) =         f2yz - f2xw;
    rot3x3(2, 0) =         f2xz - f2yw;
    rot3x3(2, 1) =         f2yz + f2xw;
    rot3x3(2, 2) = 1.0f - (f2xx + f2yy);

    return rot3x3;
}


LQuaternion LQuaternion::fromRotationMatrix(const LMatrix3x3 &rot3x3)
{
    // Algorithm from:
    // http://www.j3d.org/matrix_faq/matrfaq_latest.html#Q55

    float scalar;
    float axis[3];

    const float trace = rot3x3(0, 0) + rot3x3(1, 1) + rot3x3(2, 2);
    if (trace > 0.00000001f) {
        const float s = 2.0f * std::sqrt(trace + 1.0f);
        scalar = 0.25f * s;
        axis[0] = (rot3x3(2, 1) - rot3x3(1, 2)) / s;
        axis[1] = (rot3x3(0, 2) - rot3x3(2, 0)) / s;
        axis[2] = (rot3x3(1, 0) - rot3x3(0, 1)) / s;
    } else {
        constexpr int s_next[3] = { 1, 2, 0 };
        int i = 0;
        if (rot3x3(1, 1) > rot3x3(0, 0))
            i = 1;
        if (rot3x3(2, 2) > rot3x3(i, i))
            i = 2;
        int j = s_next[i];
        int k = s_next[j];

        const float s = 2.0f * std::sqrt(rot3x3(i, i) - rot3x3(j, j) - rot3x3(k, k) + 1.0f);
        axis[i] = 0.25f * s;
        scalar = (rot3x3(k, j) - rot3x3(j, k)) / s;
        axis[j] = (rot3x3(j, i) + rot3x3(i, j)) / s;
        axis[k] = (rot3x3(k, i) + rot3x3(i, k)) / s;
    }

    return LQuaternion(scalar, axis[0], axis[1], axis[2]);
}

auto LQuaternion::toAxes() const -> Axes
{
    const LMatrix3x3 rot3x3(toRotationMatrix());

    return { {rot3x3(0, 0), rot3x3(1, 0), rot3x3(2, 0)},
            {rot3x3(0, 1), rot3x3(1, 1), rot3x3(2, 1)},
            {rot3x3(0, 2), rot3x3(1, 2), rot3x3(2, 2)} };
}


LQuaternion LQuaternion::fromAxes(Axes axes) // clazy:exclude=function-args-by-ref
{
    LMatrix3x3 rot3x3(Uninitialized);
    rot3x3(0, 0) = axes.x.x;
    rot3x3(1, 0) = axes.x.y;
    rot3x3(2, 0) = axes.x.z;
    rot3x3(0, 1) = axes.y.x;
    rot3x3(1, 1) = axes.y.y;
    rot3x3(2, 1) = axes.y.z;
    rot3x3(0, 2) = axes.z.x;
    rot3x3(1, 2) = axes.z.y;
    rot3x3(2, 2) = axes.z.z;

    return LQuaternion::fromRotationMatrix(rot3x3);
}


LQuaternion LQuaternion::fromDirection(const LVector3D &direction, const LVector3D &up)
{
    if (lFuzzyIsZero(direction.x()) && lFuzzyIsZero(direction.y()) && lFuzzyIsZero(direction.z()))
        return LQuaternion();

    const LVector3D zAxis(direction.normalized());
    LVector3D xAxis(LVector3D::crossProduct(up, zAxis));
    if (lFuzzyIsZero(xAxis.lengthSquared())) {
        // collinear or invalid up vector; derive shortest arc to new direction
        return LQuaternion::rotationTo(LVector3D(0.0f, 0.0f, 1.0f), zAxis);
    }

    xAxis.normalize();
    const LVector3D yAxis(LVector3D::crossProduct(zAxis, xAxis));

    return LQuaternion::fromAxes(xAxis, yAxis, zAxis);
}

LQuaternion LQuaternion::rotationTo(const LVector3D &from, const LVector3D &to)
{
    // Based on Stan Melax's article in Game Programming Gems

    const LVector3D v0(from.normalized());
    const LVector3D v1(to.normalized());

    float d = LVector3D::dotProduct(v0, v1) + 1.0f;

    // if dest vector is close to the inverse of source vector, ANY axis of rotation is valid
    if (lFuzzyIsZero(d)) {
        LVector3D axis = LVector3D::crossProduct(LVector3D(1.0f, 0.0f, 0.0f), v0);
        if (lFuzzyIsZero(axis.lengthSquared()))
            axis = LVector3D::crossProduct(LVector3D(0.0f, 1.0f, 0.0f), v0);
        axis.normalize();

        // same as LQuaternion::fromAxisAndAngle(axis, 180.0f)
        return LQuaternion(0.0f, axis.x(), axis.y(), axis.z());
    }

    d = std::sqrt(2.0f * d);
    const LVector3D axis(LVector3D::crossProduct(v0, v1) / d);

    return LQuaternion(d * 0.5f, axis).normalized();
}

LQuaternion LQuaternion::slerp
    (const LQuaternion &q1, const LQuaternion &q2, float t)
{
    // Handle the easy cases first.
    if (t <= 0.0f)
        return q1;
    else if (t >= 1.0f)
        return q2;

    // Determine the angle between the two quaternions.
    LQuaternion q2b(q2);
    float dot = LQuaternion::dotProduct(q1, q2);
    if (dot < 0.0f) {
        q2b = -q2b;
        dot = -dot;
    }

    // Get the scale factors.  If they are too small,
    // then revert to simple linear interpolation.
    float factor1 = 1.0f - t;
    float factor2 = t;
    if ((1.0f - dot) > 0.0000001) {
        float angle = std::acos(dot);
        float sinOfAngle = std::sin(angle);
        if (sinOfAngle > 0.0000001) {
            factor1 = std::sin((1.0f - t) * angle) / sinOfAngle;
            factor2 = std::sin(t * angle) / sinOfAngle;
        }
    }

    // Construct the result quaternion.
    return q1 * factor1 + q2b * factor2;
}

LQuaternion LQuaternion::nlerp
    (const LQuaternion &q1, const LQuaternion &q2, float t)
{
    // Handle the easy cases first.
    if (t <= 0.0f)
        return q1;
    else if (t >= 1.0f)
        return q2;

    // Determine the angle between the two quaternions.
    LQuaternion q2b(q2);
    float dot = LQuaternion::dotProduct(q1, q2);
    if (dot < 0.0f)
        q2b = -q2b;

    // Perform the linear interpolation.
    return (q1 * (1.0f - t) + q2b * t).normalized();
}


LK_NAMESPACE_END