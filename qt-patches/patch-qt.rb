#!/usr/bin/env ruby

if Dir.pwd =~ /qt-(mac|x11|win|all)-(opensource|commercial)-src-(\d.\d.\d)/
  QT_OS = $1
  QT_VERSION = $3
else
  if Dir.pwd =~ /qt/ # qt.git
    QT_OS = 'all'
    QT_VERSION = '4.5.0'
  else
    raise "You're supposed to run patch-qt.rb from Qt base directory."
  end
end

def os_applicable(os)
  return true if os == "all"
  return true if QT_OS == "all"
  return true if os == QT_OS
  false
end

def version_applicable(ver)
  return true if ver == "all"
  return true if ver == QT_VERSION
  return true if QT_VERSION[0, ver.length] == ver
  false
end

def is_patch_applicable(file_name)
  if file_name =~ /^(\d+)-(all|win|x11|mac)-(all|\d\.\d(?:\.\d)?)-(.+)\.patch$/
    os = $2
    qt_ver = $3
    return os_applicable(os) && version_applicable(qt_ver)
  else
    return false
  end
end

$failed_patches = []

def apply_patch(file_name)
  puts "Applying #{file_name}"
  output = `cat "#{file_name}" | patch -p1`
  puts output
  $failed_patches += [file_name] if output =~ /FAILED/
end

QT_PATCHES_DIR = File.dirname(__FILE__)
patches = Dir.new(QT_PATCHES_DIR).entries.find_all { |e| e =~ /\.patch$/ }
patches = patches.find_all { |e| is_patch_applicable(e) }
patches.each { |e| apply_patch("#{QT_PATCHES_DIR}/#{e}") }

if !$failed_patches.empty?
  puts "\n\n*** Some patches were not applied ***"
  p $failed_patches
end

