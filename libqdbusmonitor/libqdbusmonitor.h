#ifndef LIQDBUSMONITOR_H
#define LIQDBUSMONITOR_H

#ifdef BUILD_LIBQDBUSMONITOR
#define LIBQDBUSMONITOR_API Q_DECL_EXPORT
#else
#define LIBQDBUSMONITOR_API Q_DECL_IMPORT
#endif

#endif // LIQDBUSMONITOR_H