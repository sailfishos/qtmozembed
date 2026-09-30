/* SPDX-FileCopyrightText: 2026 Jolla Mobile Ltd
 * SPDX-License-Identifier: MPL-2.0
 */

// This DSO supplies the legacy SONAME and loads libqt5embedwidget.so.2.
// The latter preserves the context/settings ABI used by Sailfish WebView 1.
// The removed raw QtMoz view and rendering APIs are not provided here.
