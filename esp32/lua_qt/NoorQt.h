// ╔══════════════════════════════════════════════════════════════════════════╗
// ║  NoorQt.h — Master include for all NoorQt modules                        ║
// ║  Include this one header to get the full NoorQt framework.               ║
// ║  MIT License — NoorRobot / OpenNoorIlm                                  ║
// ╚══════════════════════════════════════════════════════════════════════════╝
#pragma once

// ── Core ─────────────────────────────────────────────────────────────────────
#include "QObject.h"       // QObject, QVariant, signals/slots, meta-object
#include "QGeometry.h"     // QColor, QFont, QPoint, QSize, QRect, QTimer, QDateTime
#include "QPainter.h"      // QPainter, QPen, QBrush, gradients, paths
#include "QWidget.h"       // QWidget, QPalette, QSizePolicy
#include "QLayout.h"       // QHBoxLayout, QVBoxLayout, QGridLayout, QFormLayout
#include "QWidgets.h"      // 20+ widget classes: QPushButton, QLabel, QSlider...

// ── Application ──────────────────────────────────────────────────────────────
#include "QApplication.h"  // QApplication, QGuiApplication, QCoreApplication,
                            // QScreen, QClipboard, QSettings

// ── Data / Models ─────────────────────────────────────────────────────────────
#include "QModel.h"        // QStandardItemModel, QAbstractItemModel, QSortFilterProxyModel

// ── Filesystem ───────────────────────────────────────────────────────────────
#include "QFile.h"         // QFile, QDir, QFileInfo, QTextStream, QDataStream, QFileSystemWatcher

// ── Threading ─────────────────────────────────────────────────────────────────
#include "QThread.h"       // QThread, QMutex, QMutexLocker, QSemaphore, QWaitCondition,
                            // QThreadPool, QFuture, QtConcurrent::run()

// ── Networking ────────────────────────────────────────────────────────────────
#include "QNetwork.h"      // QNetworkAccessManager, QNetworkRequest, QNetworkReply,
                            // QTcpSocket, QUdpSocket, QDnsLookup, QHostAddress

// ── Multimedia ────────────────────────────────────────────────────────────────
#include "QMultimedia.h"   // QMediaPlayer, QAudioSink, QAudioOutput, QSoundEffect,
                            // QAudioFormat  — backed by ESP32 I2S DAC + PAM8403

// ── Animation ─────────────────────────────────────────────────────────────────
#include "QAnimation.h"    // QPropertyAnimation, QVariantAnimation, QEasingCurve,
                            // QSequentialAnimationGroup, QParallelAnimationGroup

// ── SQL ───────────────────────────────────────────────────────────────────────
#include "QSql.h"          // QSqlDatabase, QSqlQuery, QSqlRecord, QSqlField, QSqlError
                            // Backed by SQLite (if esp32-arduino-sqlite3 installed)
                            // or JSON flat-file store fallback

// ── NoorQt version ────────────────────────────────────────────────────────────
#define NOORQT_ALL_MODULES 1
#define NOORQT_VERSION "1.1.0"
