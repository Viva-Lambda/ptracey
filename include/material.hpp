#pragma once

#include <color/spectrum.hpp>
#include <common.hpp>
#include <pdf.hpp>
#include <texture.hpp>
#include <utils.hpp>
using namespace ptracey;
namespace ptracey {

struct scatter_record {
  ray specular_ray;
  bool is_specular;
  color attenuation;
  shared_ptr<pdf> pdf_ptr;
};
class material {
public:
  virtual color emitted(const ray &r_in,
                        const hit_record &rec, Real u,
                        Real v, const point3 &p) const {
    return color(0.0);
  }
  virtual bool scatter(const ray &r_in,
                       const hit_record &rec,
                       scatter_record &srec) const {
    return false;
  }
  virtual Real scattering_pdf(const ray &r_in,
                              const hit_record &rec,
                              const ray &scattered) const {
    return 0.0;
  }
};
class lambertian : public material {
public:
  lambertian(const spectrum &a)
      : albedo(make_shared<solid_color>(a)) {}
  lambertian(shared_ptr<texture> a) : albedo(a) {}
  bool scatter(const ray &r_in, const hit_record &rec,
               scatter_record &srec) const override {
    srec.is_specular = false;
    srec.attenuation = albedo->value(rec.u, rec.v, rec.p,
                                     r_in.wavelength());
    srec.pdf_ptr = make_shared<cosine_pdf>(rec.normal);
    return true;
  }
  Real scattering_pdf(const ray &r_in,
                      const hit_record &rec,
                      const ray &scattered) const override {
    auto cosine =
        dot(rec.normal, unit_vector(scattered.direction()));
    return cosine < 0 ? 0 : cosine / M_PI;
  }

public:
  shared_ptr<texture> albedo;
};
class metal : public material {
public:
  metal(const spectrum &a, Real f)
      : albedo(make_shared<solid_color>(a)),
        fuzz(f < 1 ? f : 1) {}

  bool scatter(const ray &r_in, const hit_record &rec,
               scatter_record &srec) const override {
    vec3 reflected =
        reflect(unit_vector(r_in.direction()), rec.normal);
    srec.specular_ray = ray(
        rec.p, reflected + fuzz * random_in_unit_sphere(),
        r_in.time(), r_in.wavelength());
    srec.attenuation =
        albedo->value(rec.u, rec.v, rec.p,
                      srec.specular_ray.wavelength());
    srec.is_specular = true;
    srec.pdf_ptr = nullptr;
    return true;
  }

public:
  shared_ptr<texture> albedo;
  Real fuzz;
};
class dielectric : public material {
public:
  dielectric(Real index_of_refraction)
      : ir(index_of_refraction) {}

  bool scatter(const ray &r_in, const hit_record &rec,
               scatter_record &srec) const override {
    srec.is_specular = true;
    srec.pdf_ptr = nullptr;
    color white(1.0);
    srec.attenuation = white;
    Real refraction_ratio =
        rec.front_face ? (1.0 / ir) : ir;

    vec3 unit_direction = unit_vector(r_in.direction());
    Real cos_theta =
        fmin(dot(-unit_direction, rec.normal), 1.0);
    Real sin_theta = sqrt(1.0 - cos_theta * cos_theta);

    bool cannot_refract =
        refraction_ratio * sin_theta > 1.0;
    vec3 direction;

    if (cannot_refract ||
        reflectance(cos_theta, refraction_ratio) >
            random_real())
      direction = reflect(unit_direction, rec.normal);
    else
      direction = refract(unit_direction, rec.normal,
                          refraction_ratio);

    srec.specular_ray = ray(rec.p, direction, r_in.time(),
                            r_in.wavelength());
    return true;
  }

public:
  Real ir; // Index of Refraction

private:
  static Real reflectance(Real cosine, Real ref_idx) {
    // Use Schlick's approximation for reflectance.
    auto r0 = (1 - ref_idx) / (1 + ref_idx);
    r0 = r0 * r0;
    return r0 + (1 - r0) * pow((1 - cosine), 5);
  }
};
class diffuse_light : public material {
public:
  diffuse_light(shared_ptr<texture> a) : emit(a) {}
  diffuse_light(const spectrum &c)
      : emit(make_shared<solid_color>(c)) {}

  color emitted(const ray &r_in, const hit_record &rec,
                Real u, Real v,
                const point3 &p) const override {
    if (!rec.front_face)
      return color(0.0);
    return emit->value(u, v, p, r_in.wavelength());
  }

public:
  shared_ptr<texture> emit;
};
class isotropic : public material {
public:
  isotropic(const spectrum &c)
      : albedo(make_shared<solid_color>(c)) {}
  isotropic(shared_ptr<texture> a) : albedo(a) {}

  bool scatter(const ray &r_in, const hit_record &rec,
               scatter_record &srec) const override {
    srec.specular_ray = ray(rec.p, random_in_unit_sphere(),
                            r_in.time(), r_in.wavelength());
    srec.attenuation =
        albedo->value(rec.u, rec.v, rec.p,
                      srec.specular_ray.wavelength());
    srec.is_specular = true;
    srec.pdf_ptr = nullptr;
    return true;
  }

public:
  shared_ptr<texture> albedo;
};
}
