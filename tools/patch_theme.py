#!/usr/bin/env python3
"""
Patch GetLikhi theme files with Customizer getters, navigation and security enhancements.
"""

import os
import sys

THEME_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'website', 'wordpress-theme', 'likhi')

def patch_front_page():
    fp = os.path.join(THEME_DIR, 'front-page.php')
    with open(fp, 'r', encoding='utf-8') as f:
        content = f.read()

    # 1. Hero Eyebrow
    old_badge = """          <div class="hero-badge">
            <span class="hero-badge-dot" aria-hidden="true"></span>
            Free · Offline · Native Windows App
          </div>"""
    new_badge = """          <div class="hero-badge">
            <span class="hero-badge-dot" aria-hidden="true"></span>
            <?php echo esc_html(likhi_get_hero_eyebrow()); ?>
          </div>"""
    if old_badge in content:
        content = content.replace(old_badge, new_badge, 1)

    # 2. Hero Headings & Subheadings
    old_headings = """          <h1 id="hero-heading" class="hero-headline">
            সহজে বাংলা টাইপ করুন Windows-এ<br>
            <span class="hero-headline-highlight">Type naturally. Write Bangla.</span>
          </h1>

          <p class="hero-subheadline">বাংলায় লিখুন, স্বাভাবিক উচ্চারণে।</p>

          <p class="hero-desc">
            Likhi (লিখি) — আধুনিক, দ্রুত ও বুদ্ধিমান বাংলা টাইপিং সফটওয়্যার। কোনো জটিল নিয়ম শেখার দরকার নেই — আপনার স্বাভাবিক টাইপিং প্যাটার্ন থেকেই লিখি বুঝে নেবে আপনি কী লিখতে চান। Windows 10 ও 11-এর জন্য সম্পূর্ণ ফ্রি ও নিরাপদ।
          </p>"""
    new_headings = """          <h1 id="hero-heading" class="hero-headline">
            <?php echo esc_html(likhi_get_hero_heading()); ?><br>
            <span class="hero-headline-highlight"><?php echo esc_html(likhi_get_hero_highlight()); ?></span>
          </h1>

          <p class="hero-subheadline"><?php echo esc_html(likhi_get_hero_subheadline()); ?></p>

          <p class="hero-desc">
            <?php echo esc_html(likhi_get_hero_desc()); ?>
          </p>"""
    if old_headings in content:
        content = content.replace(old_headings, new_headings, 1)

    # 3. Hero Actions
    old_actions = """          <div class="hero-actions">
            <a class="btn btn-primary btn-lg" href="<?php echo esc_url(home_url('/download')); ?>">
              <svg class="btn-icon" viewBox="0 0 20 20" fill="currentColor" aria-hidden="true"><path d="M10 2a1 1 0 011 1v8.586l2.293-2.293a1 1 0 111.414 1.414l-4 4a1 1 0 01-1.414 0l-4-4a1 1 0 111.414-1.414L9 11.586V3a1 1 0 011-1z"/><path d="M3 17a1 1 0 011-1h12a1 1 0 110 2H4a1 1 0 01-1-1z"/></svg>
              Download Likhi — Free →
            </a>
            <a class="btn btn-secondary btn-lg" href="#why-likhi">Why Likhi? →</a>
          </div>"""
    new_actions = """          <?php if (likhi_get_show_hero_cta()) : ?>
          <div class="hero-actions">
            <a class="btn btn-primary btn-lg" href="<?php echo esc_url(likhi_get_hero_primary_url()); ?>">
              <svg class="btn-icon" viewBox="0 0 20 20" fill="currentColor" aria-hidden="true"><path d="M10 2a1 1 0 011 1v8.586l2.293-2.293a1 1 0 111.414 1.414l-4 4a1 1 0 01-1.414 0l-4-4a1 1 0 111.414-1.414L9 11.586V3a1 1 0 011-1z"/><path d="M3 17a1 1 0 011-1h12a1 1 0 110 2H4a1 1 0 01-1-1z"/></svg>
              <?php echo esc_html(likhi_get_hero_primary_text()); ?>
            </a>
            <a class="btn btn-secondary btn-lg" href="<?php echo esc_url(likhi_get_hero_secondary_url()); ?>">
              <?php echo esc_html(likhi_get_hero_secondary_text()); ?>
            </a>
          </div>
          <?php endif; ?>"""
    if old_actions in content:
        content = content.replace(old_actions, new_actions, 1)

    # 4. Hero Social Proof
    old_proof = """          <div class="hero-social-proof">
            <div class="hero-proof-icons" aria-hidden="true">
              🪟 Windows 10 & 11 · ⚡ &lt;4ms startup · 🔒 Works Offline
            </div>
          </div>"""
    new_proof = """          <div class="hero-social-proof">
            <div class="hero-proof-icons" aria-hidden="true">
              <?php echo esc_html(likhi_get_hero_social_proof()); ?>
            </div>
          </div>"""
    if old_proof in content:
        content = content.replace(old_proof, new_proof, 1)

    # 5. Bottom Download CTA
    old_cta_btn = """          <a class="btn btn-primary btn-xl" href="<?php echo esc_url(home_url('/download')); ?>">
            <svg class="btn-icon" viewBox="0 0 20 20" fill="currentColor" aria-hidden="true"><path d="M10 2a1 1 0 011 1v8.586l2.293-2.293a1 1 0 111.414 1.414l-4 4a1 1 0 01-1.414 0l-4-4a1 1 0 111.414-1.414L9 11.586V3a1 1 0 011-1z"/><path d="M3 17a1 1 0 011-1h12a1 1 0 110 2H4a1 1 0 01-1-1z"/></svg>
            Download Likhi — Free
          </a>"""
    new_cta_btn = """          <a class="btn btn-primary btn-xl" href="<?php echo esc_url(likhi_get_download_page_url()); ?>">
            <svg class="btn-icon" viewBox="0 0 20 20" fill="currentColor" aria-hidden="true"><path d="M10 2a1 1 0 011 1v8.586l2.293-2.293a1 1 0 111.414 1.414l-4 4a1 1 0 01-1.414 0l-4-4a1 1 0 111.414-1.414L9 11.586V3a1 1 0 011-1z"/><path d="M3 17a1 1 0 011-1h12a1 1 0 110 2H4a1 1 0 01-1-1z"/></svg>
            <?php echo esc_html(likhi_get_download_btn_text()); ?>
          </a>"""
    if old_cta_btn in content:
        content = content.replace(old_cta_btn, new_cta_btn, 1)

    with open(fp, 'w', encoding='utf-8') as f:
        f.write(content)
    print("Patched front-page.php successfully.")

def patch_style_css():
    sp = os.path.join(THEME_DIR, 'style.css')
    with open(sp, 'r', encoding='utf-8') as f:
        css = f.read()

    # Add active navigation classes and submenu styling
    if '.nav-links li.current-menu-item > a' not in css:
        menu_css = """
/* ============================================================
   WORDPRESS MENU ENHANCEMENTS: ACTIVE STATES & SUBMENUS
   ============================================================ */
.nav-links li {
  position: relative;
}

.nav-links li.current-menu-item > a,
.nav-links li.current_page_item > a,
.nav-links li.current-menu-ancestor > a,
.nav-links a.active {
  color: var(--likhi-active-menu, var(--likhi-primary, var(--color-primary, #1A6FDB))) !important;
  font-weight: 600 !important;
}

.nav-mobile-links li.current-menu-item > a,
.nav-mobile-links li.current_page_item > a,
.nav-mobile-links a.active {
  color: var(--likhi-active-menu, var(--likhi-primary, var(--color-primary, #1A6FDB))) !important;
  background-color: rgba(26, 111, 219, 0.08) !important;
  font-weight: 600 !important;
}

/* Desktop Submenu Dropdown */
.nav-links ul.sub-menu {
  display: none;
  position: absolute;
  top: 100%;
  left: 0;
  min-width: 200px;
  background: var(--likhi-header-bg, #FFFFFF);
  border: 1px solid rgba(0, 0, 0, 0.08);
  border-radius: var(--likhi-border-radius, 8px);
  box-shadow: 0 10px 25px rgba(0, 0, 0, 0.1);
  padding: 0.5rem 0;
  margin: 0;
  list-style: none;
  z-index: 1000;
}

.nav-links li:hover > ul.sub-menu,
.nav-links li:focus-within > ul.sub-menu {
  display: block;
}

.nav-links ul.sub-menu li {
  width: 100%;
}

.nav-links ul.sub-menu a {
  display: block;
  padding: 0.5rem 1rem;
  font-size: 0.875rem;
  color: var(--likhi-text, #1A1A2E);
  white-space: nowrap;
  transition: background 0.15s ease, color 0.15s ease;
}

.nav-links ul.sub-menu a:hover,
.nav-links ul.sub-menu li.current-menu-item > a {
  background: rgba(26, 111, 219, 0.06);
  color: var(--likhi-primary, #1A6FDB) !important;
}

/* Mobile Submenu */
.nav-mobile-links ul.sub-menu {
  list-style: none;
  padding-left: 1.25rem;
  margin: 0.25rem 0;
}

.nav-mobile-links ul.sub-menu a {
  font-size: 0.95rem;
  padding: 0.4rem 0.75rem;
  opacity: 0.9;
}
"""
        css += menu_css
        with open(sp, 'w', encoding='utf-8') as f:
            f.write(css)
        print("Patched style.css successfully.")

if __name__ == '__main__':
    patch_front_page()
    patch_style_css()
