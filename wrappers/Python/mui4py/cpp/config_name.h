/*****************************************************************************
* Multiscale Universal Interface Code Coupling Library                       *
*                                                                            *
* Copyright (C) 2023 C. Richardson, E. R. Fernandez                          *
*                                                                            *
* This software is jointly licensed under the Apache License, Version 2.0    *
* and the GNU General Public License version 3, you may use it according     *
* to either.                                                                 *
*                                                                            *
* ** Apache License, version 2.0 **                                          *
*                                                                            *
* Licensed under the Apache License, Version 2.0 (the "License");            *
* you may not use this file except in compliance with the License.           *
* You may obtain a copy of the License at                                    *
*                                                                            *
* http://www.apache.org/licenses/LICENSE-2.0                                 *
*                                                                            *
* Unless required by applicable law or agreed to in writing, software        *
* distributed under the License is distributed on an "AS IS" BASIS,          *
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.   *
* See the License for the specific language governing permissions and        *
* limitations under the License.                                             *
*                                                                            *
* ** GNU General Public License, version 3 **                                *
*                                                                            *
* This program is free software: you can redistribute it and/or modify       *
* it under the terms of the GNU General Public License as published by       *
* the Free Software Foundation, either version 3 of the License, or          *
* (at your option) any later version.                                        *
*                                                                            *
* This program is distributed in the hope that it will be useful,            *
* but WITHOUT ANY WARRANTY; without even the implied warranty of             *
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the              *
* GNU General Public License for more details.                               *
*                                                                            *
* You should have received a copy of the GNU General Public License          *
* along with this program.  If not, see <http://www.gnu.org/licenses/>.      *
*****************************************************************************/

/**
 * @file config_name.h
 * @author C. Richardson, E. R. Fernandez
 * @date 20 January 2023
 * @brief Config names for MUI Python wrapper.
 */

#include <mui.h>
#include <string>

template <typename CONFIG>
std::string config_name()
{
    if (std::is_same<CONFIG, mui::mui_config_1d>())
        return "1d_f64_i32";
    if (std::is_same<CONFIG, mui::mui_config_2d>())
        return "2d_f64_i32";
    if (std::is_same<CONFIG, mui::mui_config_3d>())
        return "3d_f64_i32";
    if (std::is_same<CONFIG, mui::mui_config_1dx>())
        return "1d_f64_i64";
    if (std::is_same<CONFIG, mui::mui_config_2dx>())
        return "2d_f64_i64";
    if (std::is_same<CONFIG, mui::mui_config_3dx>())
        return "3d_f64_i64";
    if (std::is_same<CONFIG, mui::mui_config_1f>())
        return "1d_f32_i32";
    if (std::is_same<CONFIG, mui::mui_config_2f>())
        return "2d_f32_i32";
    if (std::is_same<CONFIG, mui::mui_config_3f>())
        return "3d_f32_i32";
    if (std::is_same<CONFIG, mui::mui_config_1fx>())
        return "1d_f32_i64";
    if (std::is_same<CONFIG, mui::mui_config_2fx>())
        return "2d_f32_i64";
    if (std::is_same<CONFIG, mui::mui_config_3fx>())
        return "3d_f32_i64";
    throw std::runtime_error("Invalid config type");
}

template <typename T>
std::string type_name()
{
    if (std::is_same<T, double>::value)
        return "double";
    if (std::is_same<T, float>::value)
        return "float";
    if (std::is_same<T, std::int32_t>::value)
        return "int32_t";
    if (std::is_same<T, std::int64_t>::value)
        return "int64_t";
    if (std::is_same<T, std::string>::value)
        return "string";
    throw std::runtime_error("Invalid type");
}
