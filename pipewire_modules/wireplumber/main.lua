-- WirePlumber configuration for Audio Framework

-- 导入必要的模块
local config = require('config')
local device = require('device')
local link = require('link')
local log = require('log')
local node = require('node')
local policy = require('policy')
local session = require('session')

-- 配置日志级别
log.level = "info"

-- 设备配置
local device_config = {
  -- 自动连接新设备
  auto_connect = true,
  -- 默认采样率
  default_sample_rate = 48000,
  -- 默认缓冲区大小
  default_buffer_size = 1024,
  -- 启用实时调度
  enable_realtime = true,
  -- 实时优先级
  rt_priority = 80
}

-- 会话配置
local session_config = {
  -- 默认音频路由策略
  default_routing_policy = "alsa_pcm",
  -- 启用蓝牙音频
  enable_bluetooth = true,
  -- 启用DSP处理
  enable_dsp = true
}

-- 初始化设备管理器
local function init_device_manager()
  log.info("Initializing device manager...")

  -- 应用设备配置
  device.apply_config(device_config)

  -- 监听设备事件
  device.on('added', function(dev)
    log.info("Device added: " .. dev.name)
    -- 自动配置新设备
    configure_device(dev)
  end)

  device.on('removed', function(dev)
    log.info("Device removed: " .. dev.name)
  end)

  device.on('changed', function(dev)
    log.info("Device changed: " .. dev.name)
  end)
end

-- 配置音频设备
local function configure_device(dev)
  if dev.type == "audio" then
    log.info("Configuring audio device: " .. dev.name)

    -- 设置采样率
    if not dev.properties["audio.sample-rate"] then
      dev.properties["audio.sample-rate"] = device_config.default_sample_rate
    end

    -- 设置缓冲区大小
    if not dev.properties["audio.buffer-size"] then
      dev.properties["audio.buffer-size"] = device_config.default_buffer_size
    end

    -- 启用实时调度
    if device_config.enable_realtime then
      dev.properties["audio.rt-priority"] = device_config.rt_priority
    end

    -- 应用配置
    dev:configure()
  end
end

-- 初始化路由策略
local function init_routing_policy()
  log.info("Initializing routing policy...")

  -- 设置默认路由策略
  policy.set_default_policy(session_config.default_routing_policy)

  -- 配置ALSA路由
  policy.add_policy("alsa_pcm", {
    -- 优先使用硬件设备
    prefer_hardware = true,
    -- 自动重路由
    auto_redirect = true,
    -- 支持的格式
    supported_formats = {
      "S16_LE",
      "S32_LE",
      "FLOAT32_LE"
    },
    supported_codecs = {
      "dolby-atmos",
      "dolby-eac3",
      "aac",
      "lc3",
      "ogg",
      "alac",
      "flac"
    }
  })

  -- 配置蓝牙音频路由
  if session_config.enable_bluetooth then
    policy.add_policy("bluez", {
      -- 蓝牙设备优先级较低
      priority = 50,
      -- 自动连接A2DP设备
      auto_connect_a2dp = true,
      -- 支持的编解码器
      supported_codecs = {
        "sbc",
        "aac",
        "aptx",
        "lc3",
        "ogg",
        "alac",
        "flac",
        "dolby-atmos",
        "dolby-eac3"
      }
    })
  end
end

-- 初始化DSP处理
local function init_dsp()
  if session_config.enable_dsp then
    log.info("Initializing DSP processing...")

    -- 加载flow DSP模块
    local dsp = require('dsp')

    -- 配置全局DSP链
    dsp.configure_global_chain({
      {
        type = "equalizer",
        enabled = true,
        params = {
          bands = {
            { freq = 60, gain = 0 },
            { freq = 170, gain = 0 },
            { freq = 310, gain = 0 },
            { freq = 600, gain = 0 },
            { freq = 1000, gain = 0 },
            { freq = 3000, gain = 0 },
            { freq = 6000, gain = 0 },
            { freq = 12000, gain = 0 },
            { freq = 14000, gain = 0 },
            { freq = 16000, gain = 0 }
          }
        }
      },
      {
        type = "compressor",
        enabled = true,
        params = {
          threshold = -18.0,
          ratio = 4.0,
          attack = 10.0,
          release = 100.0
        }
      },
      {
        type = "reverb",
        enabled = false,
        params = {
          room_size = 0.5,
          damp = 0.5,
          wet = 0.3,
          dry = 0.7
        }
      }
    })
  end
end

-- 初始化playerctld集成
local function init_playerctld()
  log.info("Initializing playerctld integration...")

  -- 导入D-Bus模块
  local dbus = require('dbus')

  -- 连接到playerctld服务
  local playerctl = dbus.connect('org.mpris.MediaPlayer2.playerctld', '/org/mpris/MediaPlayer2')

  -- 监听播放器事件
  playerctl.on('PropertiesChanged', function(interface, properties, invalidated)
    if properties["PlaybackStatus"] then
      log.info("Playback status changed: " .. properties["PlaybackStatus"])
      -- 根据播放状态调整音频路由
      if properties["PlaybackStatus"] == "Playing" then
        policy.redirect_audio_to_player("playerctld")
      end
    end
  end)
end

-- 主初始化函数
local function main()
  log.info("Starting WirePlumber configuration for Audio Framework...")

  -- 初始化设备管理器
  init_device_manager()

  -- 初始化路由策略
  init_routing_policy()

  -- 初始化DSP处理
  init_dsp()

  -- 初始化playerctld集成
  init_playerctld()

  log.info("WirePlumber configuration completed successfully")
end

-- 启动主函数
main()