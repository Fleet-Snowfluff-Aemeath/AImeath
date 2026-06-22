<script setup>
import { useRouter } from 'vue-router'

const props = defineProps({
  title: String,
  items: Array,
})
const router = useRouter()

const hrefFor = (item) => {
  if (item.url.startsWith('/')) return router.resolve(item.url).href
  return item.url
}
</script>

<template>
  <section class="nav-card">
    <h2 class="nav-card__title">{{ title }}</h2>
    <div class="nav-card__list">
      <a
        v-for="item in items"
        :key="item.name"
        class="nav-card__item"
        :href="hrefFor(item)"
        :title="item.desc"
        target="_blank"
        rel="noopener noreferrer"
      >
        <span class="nav-card__item-name">{{ item.name }}</span>
        <span v-if="item.desc" class="nav-card__item-desc">{{ item.desc }}</span>
      </a>
    </div>
  </section>
</template>

<style scoped>
.nav-card {
  background: #fff;
  border-radius: 12px;
  padding: 24px;
  box-shadow: 0 1px 3px rgba(0, 0, 0, 0.08);
}
.nav-card__title {
  margin: 0 0 16px;
  font-size: 18px;
  font-weight: 600;
  color: #1a1a2e;
  padding-bottom: 12px;
  border-bottom: 2px solid #e8ecf4;
}
.nav-card__list { display: flex; flex-direction: column; gap: 8px; }
.nav-card__item {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 12px 16px;
  border-radius: 8px;
  text-decoration: none;
  transition: background 0.2s;
  cursor: pointer;
}
.nav-card__item:hover { background: #f0f4ff; }
.nav-card__item-name { font-weight: 500; color: #2563eb; }
.nav-card__item-desc { font-size: 13px; color: #6b7280; }
</style>
