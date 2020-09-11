Reflection
---

$$
f_s(\vec{i}, \vec{o}, \vec{m}) = f_r(\vec{i}, \vec{o}, \vec{m}) + f_t(\vec{i}, \vec{o}, \vec{m})
$$

We are going to make the following assumption for our ray tracer: a surface can be either transparent or diffuse but not both at the same time. This is because diffuse light is a spacial case of transmision for very high density participative media.

$f_r$: reflection <br>
$f_t$: transmision (or refraction)<br>

Reflection
---

$$
f_r(\vec{i}, \vec{o}, \vec{m}) =
\frac{F(\vec{i}, \vec{h_r}) G(\vec{i}, \vec{o}, \vec{h_r}) D(\vec{h_r})}
{4 |\vec{i} \cdot \vec{n}| |\vec{o} \cdot \vec{n}|}
$$

Transmision
---

$$
f_t(\vec{i}, \vec{o}, \vec{m}) =
\frac{|\vec{i} \cdot \vec{h_t}| |\vec{o} \cdot \vec{h_t}|}
{|\vec{i} \cdot \vec{n}| |\vec{o} \cdot \vec{n}|}
\frac{\eta_o^2 (1-F(\vec{i}, \vec{h_t})) G(\vec{i}, \vec{o}, \vec{h_t}) D{\vec{h_t}}}
{(\eta_i (\vec{i} \cdot \vec{h_t}) + \eta_o (\vec{o} \cdot \vec{h_t}))^2}
$$

F: Schlick Fresnel
---
$$
F(\vec{m}, \vec{i}, F_0) = F_0 + (1 - F_0)  (1 - \vec{m} \cdot \vec{i})^5
$$

In this approximate equation, $F_0$ is the base reflectance when $\theta$ is 0 degrees.

In order to be able to use $F_0$ for metalls and dielectrics, and reuse paramers, we cam compute $F_0$ as follows;

```glsl
vec3 F0 = mix(vec3(0.04), albedo, metallic);
```

That means `F0` is `(0.04, 0.04, 0.04)` for **dielectrics** which is a very common value for opaque materials. For **metals**, we use some characteristic value, which is specified in the `albedo`.



F: Exact Fresnel
---

$$
F(\vec{i}, \vec{m}) =
\frac{1}{2}
\frac{(g-c)^2}{(g+c)^2}
\left( 1+
\frac{(c(g+c)-1)^2}
{(c(g-c)+1)^2}
\right)
$$

where g:

$$
c = |\vec{i} \cdot \vec{m}| \newline
g = \sqrt{ \frac{\eta_t^2}{\eta_i^2} -1 +c^2}
$$

Important: if $g$ is imaginary, that means there is total reflection (i.e. $F = 1$).

GGX D
----
We can sample $D(\vec{m}) |\vec{m} \cdot \vec{n}|$ with the following:

$$
\theta_m = arctan \left( \frac{\alpha_g \sqrt{\xi_1}}{2 \pi \xi_1} \right)
$$
$$
\phi_m = 2 \pi \xi_2
$$

Were $\xi_1$ and $\xi_2$ are random variables between 0 and 1.

